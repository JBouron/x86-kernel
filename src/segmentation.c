#include <segmentation.h>
#include <types.h>
#include <debug.h>
#include <memory.h>
#include <kernel_map.h>
#include <kmalloc.h>
#include <acpi.h>
#include <percpu.h>
#include <addr_space.h>

// The granularity of a segment.
enum segment_granularity {
    BYTES = 0,
    PAGES = 1,
};

// The type of a segment, per Intel's manual vol 3 (3.4.5.1 table 3.1):
//  - type:accessed == 10 (type == 5, accessed == 0): Code seg, execute and read
//  - type:accessed == 2  (type == 1, accessed == 0): Data seg, R/W
enum segment_type {
    DATA = 1,
    CODE = 5,
};

union segment_descriptor_t {
    uint64_t value;
    struct {
        // Lower 15 bits of the limit.
        uint16_t limit_15_0 : 16;
        // Lower 15 bits of the base.
        uint16_t base_15_0 : 16;
        // Bits 16 to 23 of base.
        uint8_t base_23_16 : 8;
        // Has the segment been accessed ?
        uint8_t accessed : 1;
        // Type of segment.
        enum segment_type type : 3;
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
        enum segment_granularity granularity : 1;
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

// Each CPU has its own TSS since each cpu has its own kernel stack.
DECLARE_PER_CPU(struct tss, tss);

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

// Statically initialize a segment descriptor.
// @param base: The base linear address of the segment.
// @param gran: The granularity of the segment, this is an enum
// segment_granularity value.
// @param limit: The limit of the segment (bytes or pages depending on the value
// of `gran`).
// @param _type: enum segment_type indicating if the segment is a code or data
// segment.
// @param dpl: The privilege level of the segment.
#define GDT_ENTRY(base, gran, limit, _type, dpl)   \
    (union segment_descriptor_t){                  \
        .base_15_0 =  base & 0x0000FFFF,           \
        .base_23_16 = (base & 0x00FF0000) >> 16,   \
        .base_31_24 = (base & 0xFF000000) >> 24,   \
        .limit_15_0 =  limit & 0x0000FFFF,         \
        .limit_19_16 = (limit & 0x000F0000) >> 16, \
        .accessed = 0,                             \
        .type = _type,                             \
        .system = 1,                               \
        .privilege_level = dpl,                    \
        .present = 1,                              \
        .is_64_bit_segment = 0,                    \
        .operation_size = 1,                       \
        .granularity = gran                        \
    }

// Statically initialize a NUL segment.
#define GDT_NULL_ENTRY()            \
    (union segment_descriptor_t){   \
        .value = 0,                 \
    }

// Compute the value of a segment selector.
// @param idx: The index of the segment to select.
// @param rpl: The requested privilege level.
// @return: An union segment_selector_t initialized with the provided values.
// Note: This kernel does not yet support LDTs hence the local bit is 0.
#define SEG_SEL(idx, rpl)               \
    (union segment_selector_t){         \
        .index = idx,                   \
        .is_local = 0,                  \
        .requested_priv_level = rpl,    \
    }

// The Boot GDT is the GDT used by the BSP during boot/kernel initialization. It
// contains the mininum amount of segments to perform initialization, that is:
//  - Kernel flat data segment.
//  - Kernel flat code segment.
//  - Percpu segment for the BSP.
// Later on, once paging is enabled and the number of cpus on the system is
// known, the final GDT will be allocated and the BSP will switch to it.
#define BOOT_GDT_KDATA_IDX  1
#define BOOT_GDT_KCODE_IDX  2
#define BOOT_GDT_BSP_PC_IDX 3

static union segment_descriptor_t BOOT_GDT[] __attribute__((aligned(8))) = {
    [0]                     = GDT_NULL_ENTRY(),
    [BOOT_GDT_KDATA_IDX]    = GDT_ENTRY(0x00000000, PAGES, 0xFFFFF, DATA, 0),
    [BOOT_GDT_KCODE_IDX]    = GDT_ENTRY(0x00000000, PAGES, 0xFFFFF, CODE, 0),
    // The Percpu segment is initialized in init_segmentation() prior to loading
    // the boot GDT.
    [BOOT_GDT_BSP_PC_IDX]   = GDT_NULL_ENTRY(),
};

// This is the final GDT. This GDT will contain the kernel and user segments as
// well as one percpu segment + tss segment per cpu. The GDT will be dynamically
// allocated by the BSP once the number of cpus in the system is known, paging
// is enabled and the dynamic memory allocator is initialized.
// The final GDT will have the following layout, assuming N cpus:
//   0   |   NULL entry                  |
//   1   |   Kernel Data Segment         |
//   2   |   Kernel Code Segment         |
//   3   |   User Data Segment           |
//   4   |   User Code Segment           |
//   5   |   CPU 0's percpu segment      |
//   6   |   CPU 1's percpu segment      |
//       |           ...                 |
// 5+N-1 |   CPU N's percpu segment      |
//  5+N  |   CPU 0's TSS segment         |
// 5+N+1 |   CPU 1's TSS segment         |
//       |           ...                 |
// 5+2N-1|   CPU N's TSS segment         |
static union segment_descriptor_t *GDT = NULL;
static size_t GDT_SIZE = 0;

// The following macros are defining the index of each segment in the final GDT.
#define GDT_KDATA_IDX       1
#define GDT_KCODE_IDX       2
#define GDT_UDATA_IDX       3
#define GDT_UCODE_IDX       4
// The index of the percpu segment in the GDT for a particular cpu.
// @param cpu: The ACPI index of the cpu (starting at 0).
#define GDT_PERCPU_IDX(cpu) (5 + (cpu))
// The index of the TSS segment in the GDT for a particular cpu.
// @param cpu: The ACPI index of the cpu (starting at 0).
#define GDT_TSS_IDX(cpu)    (5 + acpi_get_number_cpus() + (cpu))

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

// Set the segment registers of the current cpu to the given segments.
// @param code_seg: The code segment to use. This will be loaded in CS.
// @param data_seg: The data segment to use. This will be loaded in DS, ES, FS,
// and SS.
// @param pcpu_seg: The percpu segment to use. This will be loaded in GS.
static void set_segment_regs(union segment_selector_t code_seg,
                             union segment_selector_t data_seg,
                             union segment_selector_t pcpu_seg) {
    cpu_set_cs(&code_seg);

    cpu_set_ds(&data_seg);
    cpu_set_es(&data_seg);
    cpu_set_fs(&data_seg);
    cpu_set_ss(&data_seg);

    cpu_set_gs(&pcpu_seg);
}

void init_segmentation(void) {
    // The segmentation is initialized very early during boot _before_ paging.
    // Therefore we need to fix up the pointer to global variables as they are
    // virtual. to_phys defined in boot.S does exactly this.
    union segment_descriptor_t *gdt_phy = to_phys(&BOOT_GDT);

    // The BSP percpu segment in the Boot GDT is the only segment that is not
    // initialized, do it now.
    // The BSP uses the .percpu section from the ELF loaded into memory as its
    // percpu segment. This is because we cannot yet allocate the segment
    // (paging and dynamic allocators are not initialized).
    // NOTE: Since paging is not yet enabled, we need to use the physical
    // address of the .percpu section as the base. Once paging is enabled, we
    // will need to change the base to the physical address of the section
    // (which will be in higher half).
    uint32_t const base = (uint32_t)to_phys(SECTION_PERCPU_START_ADDR);
    uint16_t const limit = (uint16_t)SECTION_PERCPU_SIZE;
    gdt_phy[BOOT_GDT_BSP_PC_IDX] = GDT_ENTRY(base, BYTES, limit, DATA, 0);

    // Load the GDTR.
    load_gdt(gdt_phy, sizeof(BOOT_GDT));

    // Initialize the segment registers, don't trust the boot loader.
    union segment_selector_t const kdata = SEG_SEL(BOOT_GDT_KDATA_IDX, 0);
    union segment_selector_t const kcode = SEG_SEL(BOOT_GDT_KCODE_IDX, 0);
    union segment_selector_t const percpu = SEG_SEL(BOOT_GDT_BSP_PC_IDX, 0);
    set_segment_regs(kcode, kdata, percpu);
    // Note: percpu variables cannot be accessed until init_bsp_boot_percpu() is
    // called.
}

void fixup_gdt_after_paging_enable(void) {
    // When paging is finally enabled, two things need to be taken care of:
    //  - The address in the GDTR should be changed to an higher half address.
    //  - The base address of the percpu segment should be fixed as well.
    // When this function is called, we should be using the Boot GDT.
    struct gdt_desc gdtr;
    cpu_sgdt(&gdtr);
    ASSERT(gdtr.base == to_phys(&BOOT_GDT));

    // Fixup the percpu segment.
    uint32_t const base = (uint32_t)SECTION_PERCPU_START_ADDR;
    uint16_t const limit = (uint16_t)SECTION_PERCPU_SIZE;
    BOOT_GDT[BOOT_GDT_BSP_PC_IDX] = GDT_ENTRY(base, BYTES, limit, DATA, 0);
    // Reload the GS register to update the hidden copy.
    union segment_selector_t const percpu = SEG_SEL(BOOT_GDT_BSP_PC_IDX, 0);
    cpu_set_gs(&percpu);

    // Other segment registers do not need to change since the code and data
    // segments did not change (still from 0x0 to 0xFF..FF).

    // Use the higher half address of the Boot GDT in GDTR.
    load_gdt(BOOT_GDT, sizeof(BOOT_GDT));
}

void ap_init_segmentation(void) {
    // APs do not use the early GDT, only the final GDT. Make sure that, at this
    // point, the final GDT has been allocated.
    if (!GDT) {
        PANIC("Final GDT has not been allocated before waking APs.");
    }

    // Load the final GDT.
    load_gdt(GDT, GDT_SIZE * sizeof(*GDT));

    // We cannot use cpu_id() here since the percpu segment is not yet loaded
    // for this cpu.
    uint8_t const cpu = cpu_apic_id();

    // Set the segment registers.
    union segment_selector_t const kdata = SEG_SEL(GDT_KDATA_IDX, 0);
    union segment_selector_t const kcode = SEG_SEL(GDT_KCODE_IDX, 0);
    union segment_selector_t const percpu = SEG_SEL(GDT_PERCPU_IDX(cpu), 0);
    set_segment_regs(kcode, kdata, percpu);

    // From now on, this cpu can access its percpu variables.

    // Paging has been enabled manually, use switch_to_addr_space() to set up
    // all the state correctly now that we can use percpu variables.
    switch_to_addr_space(get_kernel_addr_space());
}

union segment_selector_t kernel_data_selector(void) {
    return SEG_SEL(GDT_KDATA_IDX, 0);
}

union segment_selector_t kernel_code_selector(void) {
    return SEG_SEL(GDT_KCODE_IDX, 0);
}

void initialize_trampoline_gdt(struct ap_boot_data_frame * const data_frame) {
    // Per documentation of the AP start up procedure, the APs use a temporary
    // GDT to enter protected mode. The content of this GDT is as follows:
    //      GDT[0] = <NULL entry>
    //      GDT[1] = flat data segment spanning from 0x00000000 to 0xFFFFFFFF.
    //      GDT[2] = flat code segment spanning from 0x00000000 to 0xFFFFFFFF.
    // Since the GDT used by the BSP has the same layout, we can simply copy it
    // over to the temporary GDT.
    data_frame->gdt[0] = GDT_NULL_ENTRY().value;
    data_frame->gdt[1] = GDT_ENTRY(0x0, PAGES, 0xFFFFF, DATA, 0).value;
    data_frame->gdt[2] = GDT_ENTRY(0x0, PAGES, 0xFFFFF, CODE, 0).value;

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

void init_final_gdt() {
    if (!PER_CPU_OFFSETS) {
        // allocate_aps_percpu_areas() _must_ be called before init_final_gdt.
        PANIC("Percpu areas were not allocated prior to the final GDT\n");
    }

    // 1 NULL entry, one kernel data segment, one kernel code segment, one user
    // data segment, one user code segment, one segment per cpu for per-cpu data
    // and one segment per cpu for TSS.
    uint8_t const ncpus = acpi_get_number_cpus();
    GDT_SIZE = 5 + 2 * ncpus;

    // Allocate the final GDT. Kmalloc zeroes the GDT for us.
    GDT = kmalloc(GDT_SIZE * sizeof(*GDT));
    if (!GDT) {
        PANIC("Cannot allocate final GDT\n");
    }

    // Create flat user and kernel data/code segments.
    GDT[GDT_KDATA_IDX] = GDT_ENTRY(0x0, PAGES, 0xFFFFF, DATA, 0);
    GDT[GDT_KCODE_IDX] = GDT_ENTRY(0x0, PAGES, 0xFFFFF, CODE, 0);
    GDT[GDT_UDATA_IDX] = GDT_ENTRY(0x0, PAGES, 0xFFFFF, DATA, 3);
    GDT[GDT_UCODE_IDX] = GDT_ENTRY(0x0, PAGES, 0xFFFFF, CODE, 3);

    // Add one entry per cpu to contain per-cpu copies.
    size_t const pcpu_size = SECTION_PERCPU_SIZE;
    for (uint8_t i = 0; i < ncpus; ++i) {
        uint32_t const base = (uint32_t)cpu_var(this_cpu_off, i);
        GDT[GDT_PERCPU_IDX(i)] = GDT_ENTRY(base, BYTES, pcpu_size, DATA, 0);
    }

    load_gdt(GDT, GDT_SIZE * sizeof(*GDT));

    // Reload the segment selector to use the new segment. Note: The GS segment
    // will change since the index of the percpu segment for the BSP is
    // different from the Boot GDT.
    uint8_t const cpu = cpu_id();
    union segment_selector_t const kdata = SEG_SEL(GDT_KDATA_IDX, 0);
    union segment_selector_t const kcode = SEG_SEL(GDT_KCODE_IDX, 0);
    union segment_selector_t const percpu = SEG_SEL(GDT_PERCPU_IDX(cpu), 0);
    set_segment_regs(kcode, kdata, percpu);

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

    uint8_t const cpu = cpu_id();
    struct tss * const tss = &this_cpu_var(tss);
    uint16_t const tss_index = GDT_TSS_IDX(cpu);
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
    return SEG_SEL(GDT_UCODE_IDX, 3);
}

union segment_selector_t user_data_seg_sel(void) {
    return SEG_SEL(GDT_UDATA_IDX, 3);
}

void set_segment_registers_for_kernel(void) {
    union segment_selector_t const kdata = SEG_SEL(GDT_KDATA_IDX, 0);
    union segment_selector_t const kcode = SEG_SEL(GDT_KCODE_IDX, 0);
    // NOTE /!\ We cannot use cpu_id() here because GS does not point to the
    // percpu segment yet !! This is because this function is called when
    // entring an interrupt handler.
    // TODO: There should be a check in this_cpu_var() that the correct segment
    // is loaded in %GS.
    uint8_t const cpu = cpu_apic_id();
    union segment_selector_t const percpu = SEG_SEL(GDT_PERCPU_IDX(cpu), 0);
    set_segment_regs(kcode, kdata, percpu);
}

void change_tss_esp0(void const * const new_esp0) {
    struct tss * const tss = &this_cpu_var(tss);
    tss->esp0 = (void*)new_esp0;
}

#include <segmentation.test>
