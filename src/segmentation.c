#include <segmentation.h>
#include <types.h>
#include <debug.h>
#include <memory.h>
#include <kernel_map.h>
#include <kmalloc.h>
#include <acpi.h>
#include <percpu.h>
#include <addr_space.h>

union segment_descriptor_t {
    uint64_t value;
    struct {
        // Lower 15 bits of the limit.
        uint16_t limit_15_0 : 16;
        // Lower 15 bits of the base.
        uint16_t base_15_0 : 16;
        // Bits 16 to 23 of base.
        uint8_t base_23_16 : 8;
        // Type of segment.
        uint8_t type : 4;
        // Is the segment a system segment (0) or a code/data segment (1).
        uint8_t system : 1;
        // Required privilege level to access the segment.
        uint8_t privilege_level : 2;
        // Is the segment present in the GDT/LDT.
        uint8_t present : 1;
        // Bits 16 to 19 of the limit.
        uint8_t limit_19_16 : 4;
        // Available for use by system software.
        uint8_t avail : 1;
        // Is the segment is a 64 bits segment ?
        uint8_t is_64_bit_segment : 1;
        // Is the segment a 32 bits segment ? 0 means 16 bits.
        uint8_t operation_size : 1;
        // Granularity. If set the limit is by 4K increments.
        uint8_t granularity : 1;
        // Bites 24 to 32 of base.
        uint8_t base_31_24 : 8;
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(union segment_descriptor_t) == 8, "");

// The TSS is a big structure. However, since we are doing software scheduling
// only, the only fields that we are going to use are ESP0 and SS0.
struct tss {
    union {
        uint8_t _padding[104];
        struct {
            uint32_t : 32;

            // The stack pointer to use when entering the kernel through an
            // interrupt.
            void * esp0;
            // The stack segment to use when entering the kernel through an
            // interrupt.
            union segment_selector_t ss0; 

            uint16_t : 16;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct tss) == 104, "");

// TSS descriptor for the GDT.
struct tss_descriptor {
    union {
        uint64_t value;
        struct {
            // Limit, bits 0 through 15.
            uint16_t limit_15_0 : 16;
            // Base, bits 0 through 15.
            uint16_t base_15_0 : 16;
            // Base, bits 16 through 23.
            uint8_t base_23_16 : 8;
            // Type should be the following : 1 0 B 1, where B represents the
            // busy bit.
            uint8_t type : 4;
            // Should be 0.
            uint8_t zero : 1;
            // The privilege level required to switch to this task.
            uint8_t privilege_level : 2;
            // Is the TSS present ?
            uint8_t present : 1;
            // Limit, bits 16 through 19.
            uint8_t limit_19_16 : 4;
            // Available for use by system.
            uint8_t avail : 1;
            // Should be 0.
            uint8_t zero2 : 2;
            // Granularity. If set the limit is by 4K increments.
            uint8_t granularity : 1;
            // Base, bits 24 through 31.
            uint8_t base_31_24 : 8;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct tss_descriptor) == 8, "");

// Each CPU has its own TSS since each cpu has its own kernel stack.
DECLARE_PER_CPU(struct tss, tss);

static void init_desc(union segment_descriptor_t * const desc,
                      uint32_t const base,
                      uint32_t const limit,
                      bool const code,
                      uint8_t const priv_level) {
    // The limit is only encoded on 20 bits and therefore is limited to 2^20.
    ASSERT(limit < (2 << 20));
    desc->base_15_0 =  base & 0x0000FFFF;
    desc->base_23_16 = (base & 0x00FF0000) >> 16;
    desc->base_31_24 = (base & 0xFF000000) >> 24;
    desc->limit_15_0 =  limit & 0x0000FFFF;
    desc->limit_19_16 = (limit & 0x000F0000) >> 16;

    // Set the type depending if this is a code segment or data segment. See
    // chapter 3.4.5.1 for more info about the meaning of the value of the type.
    uint8_t const type = code ? 10 : 2;
    desc->type = type;
    desc->system = 1;
    desc->privilege_level = priv_level;
    desc->present = 1;
    desc->is_64_bit_segment = 0;
    desc->operation_size = 1;
    desc->granularity = 1;
}

// This is the early GDT used during BSP boot sequence. This GDT contains two
// segments: a data segment (index 1) and a code segment (index 2). Both
// segments are flat and covering the entire linear address space.
// The reason behind having an early GDT is that later on, once paging and
// virtual memory allocation set up, we can dynamically allocate a GDT for our
// needs (percpu segments, ...).
#define EARLY_GDT_SIZE 3
static union segment_descriptor_t EARLY_GDT[EARLY_GDT_SIZE]
    __attribute__ ((aligned (8)));

// This is the final GDT. This will be dynamically allocated once we know the
// exact size of it. In particular once we know the per-cpu segments.
static union segment_descriptor_t *GDT = NULL;
// The number of _entries_ in `GDT`, counting the NULL entry.
static size_t GDT_SIZE = 0;

// The layout of the final GDT is as follows:
// 0    |   NULL entry                  |
// 1    |   Kernel Data Segment         |
// 2    |   Kernel Code Segment         |
// 3    |   First CPU's percpu segment  |
// 4    |           ...                 |
// 5    |   Last CPU's percpu segment   |
// The following defines/const provide shortcuts to indices in the final GDT.
// The index of the kernel data segment in the GDT.
static uint16_t const KERNEL_DATA_INDEX = 1;
// The index of the kernel code segment in the GDT.
static uint16_t const KERNEL_CODE_INDEX = 2;
// The index of the user data segment in the GDT.
uint16_t const USER_DATA_INDEX = 3;
// The index of the user code segment in the GDT.
uint16_t const USER_CODE_INDEX = 4;

// The index of the first cpu's percpu segment.
static uint16_t const PERCPU_SEG_START_INDEX = 5;

// Load a GDT into the GDTR of the current cpu.
// @param gdt: The linear address of the GDT to load.
// @param size: The total size in bytes of the GDT.
static void load_gdt(union segment_descriptor_t* const gdt, size_t const size) {
    ASSERT(gdt);
    struct gdt_desc const table_desc = {
        .base = gdt,
        .limit = size - 1,
    };
    cpu_lgdt(&table_desc);
}

// Get the segment descriptor for the segment containing a cpu's copy of per-cpu
// variables.
// @param cpuid: The APIC ID of the cpu to get the segment.
// @return: The segment descriptor that should be used to access cpu's copy of
// per-cpu variables.
static union segment_selector_t per_cpu_segment_selector(uint8_t const cpuid) {
    union segment_selector_t sel = {
        .index = PERCPU_SEG_START_INDEX + cpuid,
        .is_local = 0,
        .requested_priv_level = 0,
    };
    return sel;
}

// Setup the segment selectors on this cpu to use the flat kernel data and
// kernel code segments in GDT.
// @param set_percpu_seg: If true, this function will set %GS to point to the
// percpu segment of this cpu. If percpu segments have not yet been enabled
// (when this function is called from init_segmentation for instance) it must be
// false to avoid a #GP.
static void setup_segment_selectors(bool const set_percpu_seg) {
    union segment_selector_t data_seg = kernel_data_selector();
    cpu_set_ds(&data_seg);
    cpu_set_es(&data_seg);
    cpu_set_fs(&data_seg);
    cpu_set_ss(&data_seg);

    union segment_selector_t code_seg = kernel_code_selector();
    cpu_set_cs(&code_seg);

    if (set_percpu_seg) {
        // Setup the per-cpu segment.
        uint8_t const id = cpu_apic_id();
        union segment_selector_t const pcpu_seg = per_cpu_segment_selector(id);
        cpu_set_gs(&pcpu_seg);
    } else {
        // Before percpu segments are set, %GS is set to 0. This makes it easy
        // to check if percpu data is ready on a given cpu or not.
        union segment_selector_t null_sel = {.value = 0};
        cpu_set_gs(&null_sel);
    }
}

void init_segmentation(void) {
    // The segmentation is initialized very early during boot _before_ paging.
    // Therefore we need to fix up the pointer to global variables as they are
    // virtual. to_phys defined in boot.S does exactly this.
    // Get the physical address (which is also the linear address since paging
    // is not yet enabled) of the EARLY_GDT.
    union segment_descriptor_t *gdt_phy = to_phys(&EARLY_GDT);

    // Now that we have a linear/physical pointer on the EARLY_GDT we can start
    // its initialization.

    memzero(gdt_phy, sizeof(union segment_descriptor_t) * EARLY_GDT_SIZE);
    // Create flat data segment for ring 0.
    init_desc(gdt_phy + KERNEL_DATA_INDEX, 0, 0xFFFFF, false, 0);
    // Create flat code segment for ring 0.
    init_desc(gdt_phy + KERNEL_CODE_INDEX, 0, 0xFFFFF, true, 0);

    // Load the GDTR.
    load_gdt(gdt_phy, EARLY_GDT_SIZE * sizeof(*gdt_phy));

    setup_segment_selectors(false);
}

void ap_init_segmentation(void) {
    // APs do not use the early GDT, only the final GDT. Make sure that, at this
    // point, the final GDT has been allocated.
    if (!GDT) {
        PANIC("Final GDT has not been allocated before waking APs.");
    }

    // Load the final GDT.
    load_gdt(GDT, GDT_SIZE * sizeof(*GDT));

    // Re-init all segment selectors on the current cpu.
    setup_segment_selectors(true);

    // Paging has been enabled manually, use switch_to_addr_space() to set up
    // all the state correctly now that we can use percpu variables.
    switch_to_addr_space(get_kernel_addr_space());
}

union segment_selector_t kernel_data_selector(void) {
    union segment_selector_t sel = {
        .index = KERNEL_DATA_INDEX,
        .is_local = 0,
        .requested_priv_level = 0,
    };
    return sel;
}

union segment_selector_t kernel_code_selector(void) {
    union segment_selector_t sel = {
        .index = KERNEL_CODE_INDEX,
        .is_local = 0,
        .requested_priv_level = 0,
    };
    return sel;
}

void initialize_trampoline_gdt(struct ap_boot_data_frame * const data_frame) {
    // Per documentation of the AP start up procedure, the APs use a temporary
    // GDT to enter protected mode. The content of this GDT is as follows:
    //      GDT[0] = <NULL entry>
    //      GDT[1] = flat data segment spanning from 0x00000000 to 0xFFFFFFFF.
    //      GDT[2] = flat code segment spanning from 0x00000000 to 0xFFFFFFFF.
    // Since the GDT used by the BSP has the same layout, we can simply copy it
    // over to the temporary GDT.
    memcpy(data_frame->gdt, EARLY_GDT, sizeof(EARLY_GDT));

    // Initialize the GDT table descriptor in the data frame so that APs can
    // simply call the lgdt instruction without having to deal with the desc
    // computation.
    data_frame->gdt_desc.limit = sizeof(data_frame->gdt) - 1;
    // FIXME: The base address of the GDT in the table desc need to be a linear
    // address. Here we are passing the address of data_frame->gdt which assumes
    // that the data frame has been identity mapped. Ideally we would need to
    // get the physical location of the data frame and add the temp gdt offset
    // to it.
    data_frame->gdt_desc.base = &data_frame->gdt;
}

void switch_to_final_gdt(void **per_cpu_areas) {
    // 1 NULL entry, one kernel data segment, one kernel code segment, one user
    // data segment, one user code segment, one segment per cpu for per-cpu data
    // and one segment per cpu for TSS.
    uint8_t const ncpus = acpi_get_number_cpus();
    size_t const num_entries = 5 + 2 * ncpus;

    // Allocate the final GDT. Kmalloc zeroes the GDT for us.
    GDT = kmalloc(num_entries * sizeof(*GDT));
    if (!GDT) {
        PANIC("Cannot allocate final GDT\n");
    }

    // Copy the data and code segments from the early GDT. Those stay the same.
    GDT[KERNEL_DATA_INDEX] = EARLY_GDT[KERNEL_DATA_INDEX];
    GDT[KERNEL_CODE_INDEX] = EARLY_GDT[KERNEL_CODE_INDEX];

    // Prepare the user segments.
    init_desc(GDT + USER_DATA_INDEX, 0, 0xFFFFF, false, 3);
    init_desc(GDT + USER_CODE_INDEX, 0, 0xFFFFF, true, 3);

    // Add one entry per cpu to contain per-cpu copies.
    size_t const pcpu_size = SECTION_PERCPU_SIZE;
    for (uint8_t i = 0; i < ncpus; ++i) {
        union segment_descriptor_t * desc = GDT + PERCPU_SEG_START_INDEX + i;

        // FIXME: Because the default granularity is a page, the percpu segment
        // may overlap other data towards the end of it. This could be fixed by
        // adding an option to seg byte-granularity in the per-cpu segments of
        // the GDT. For now page granularity is enough.
        init_desc(desc, (uint32_t)per_cpu_areas[i], pcpu_size, false, 0);
    }

    load_gdt(GDT, num_entries * sizeof(*GDT) - 1);
    setup_segment_selectors(true); 

    // We couldn't set the curr_addr_space variable when initializing paging
    // because percpu was not initialized. Set it now.
    ASSERT(percpu_initialized());
    switch_to_addr_space(get_kernel_addr_space());
}

// Construct a TSS descriptor to point to a particular TSS.
// @param tss: The address the descriptor should point to.
// @return: A struct tss_descriptor fully initialized and ready to be inserted
// into the GDT.
struct tss_descriptor generate_tss_descriptor(struct tss const * const tss) {
    struct tss_descriptor desc;
    memzero(&desc, sizeof(desc));
    uint32_t const base = (uint32_t)(void*)tss;
    uint32_t const limit = sizeof(*tss);
    desc.base_15_0 =  base & 0x0000FFFF;
    desc.base_23_16 = (base & 0x00FF0000) >> 16;
    desc.base_31_24 = (base & 0xFF000000) >> 24;
    desc.limit_15_0 =  limit & 0x0000FFFF;
    desc.limit_19_16 = (limit & 0x000F0000) >> 16;
    // Second bit indicate if the task is busy. This is not used in this kernel
    // but must be set to 0 when creating a new TSS segment.
    desc.type = 0b1001;
    desc.privilege_level = 0;
    desc.present = 1;
    desc.granularity = 0;
    return desc;
}

void setup_tss(void) {
    // We require percpu to be initialized as the tss is stored as a percpu var.
    ASSERT(percpu_initialized());

    uint8_t const ncpus = acpi_get_number_cpus();
    uint8_t const cpu_id = this_cpu_var(cpu_id);
    uint16_t const tss_seg_start_index = PERCPU_SEG_START_INDEX + ncpus;
    struct tss * const tss = &this_cpu_var(tss);
    uint16_t const tss_index = tss_seg_start_index + cpu_id;
    union segment_descriptor_t * desc = GDT + tss_index;

    struct tss_descriptor const tss_desc = generate_tss_descriptor(tss);
    desc->value = tss_desc.value;

    // Prepare the TSS for this cpu.
    memzero(tss, sizeof(*tss));
    tss->esp0 = this_cpu_var(kernel_stack);
    tss->ss0 = cpu_read_ss();

    // Load the TSS.
    union segment_selector_t const tss_sel = {.index = tss_index};
    cpu_ltr(tss_sel);
}

union segment_selector_t user_code_seg_sel(void) {
    union segment_selector_t code_seg_sel = {
        .index = USER_CODE_INDEX,
        .is_local = false,
        .requested_priv_level = 3,
    };
    return code_seg_sel;
}

union segment_selector_t user_data_seg_sel(void) {
    union segment_selector_t data_seg_sel = {
        .index = USER_DATA_INDEX,
        .is_local = false,
        .requested_priv_level = 3,
    };
    return data_seg_sel;
}

void set_segment_registers_for_kernel(void) {
    setup_segment_selectors(true);
}

#include <segmentation.test>
