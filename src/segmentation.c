#include <segmentation.h>
#include <types.h>
#include <debug.h>
#include <memory.h>

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

#define GDT_SIZE 3
static union segment_descriptor_t GDT[GDT_SIZE] __attribute__ ((aligned (8)));

// The index of the kernel data segment in the GDT.
static uint16_t const KERNEL_DATA_INDEX = 1;

// The index of the kernel code segment in the GDT.
static uint16_t const KERNEL_CODE_INDEX = 2;

void init_segmentation(void) {
    memzero(GDT, sizeof(GDT));
    // Create flat data segment for ring 0.
    init_desc(GDT + KERNEL_DATA_INDEX, 0, 0xFFFFF, false, 0);
    // Create flat code segment for ring 0.
    init_desc(GDT + KERNEL_CODE_INDEX, 0, 0xFFFFF, true, 0);

    // Load the GDTR.
    struct gdt_desc_t const table_desc = {
        .base = GDT,
        .limit = (sizeof(union segment_descriptor_t) * GDT_SIZE) - 1,
    };
    cpu_lgdt(&table_desc);

    // Update the data segment registers.
    union segment_selector_t data_seg = {
        .index = KERNEL_DATA_INDEX,
        .is_local = 0,
        .requested_priv_level = 0,
    };
    cpu_set_ds(&data_seg);
    cpu_set_es(&data_seg);
    cpu_set_fs(&data_seg);
    cpu_set_gs(&data_seg);
    cpu_set_ss(&data_seg);

    // Update code segment register.
    union segment_selector_t code_seg = {
        .index = KERNEL_CODE_INDEX,
        .is_local = 0,
        .requested_priv_level = 0,
    };
    cpu_set_cs(&code_seg);
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
#include <segmentation.test>
