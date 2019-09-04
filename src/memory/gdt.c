#include <memory/gdt.h>
#include <memory/segment.h>
#include <asm/asm.h>
#include <utils/memory.h>
#include <utils/debug.h>

// This kernel uses a flat model, that is we are using only two segement (one
// data segment and one code segment) both covering the entire linear address
// space.

// The code segment of the kernel is at index 1 in the GDT while the data
// segment is at index 2.
uint16_t const CODE_SEGMENT_IDX = 1;
uint16_t const DATA_SEGMENT_IDX = 2;

// Right after the GDT is set up, we will load those two selectors into the
// segment registers.
uint16_t const CODE_SEGMENT_SELECTOR = CODE_SEGMENT_IDX << 3 | RING_0;
uint16_t const DATA_SEGMENT_SELECTOR = DATA_SEGMENT_IDX << 3 | RING_0;

// Add a segment to the GDT.
void
gdt_add_segment(gdt_t const gdt,
                uint16_t const index,
                uint32_t const base,
                uint32_t const size,
                uint8_t const type) {
    // The size of the segment must be 4KB granularity so that we can cover the
    // entire 32-bit addressable space.
    ASSERT(0 < index && index < 8192);
    struct segment_desc_t * const desc = gdt + index;

    // The current entry in the GDT should not be present. That would mean that
    // we want to modify the GDT entry which doesn't really make sense since we
    // are not supporting that.
    ASSERT(!desc->present);

    // Encode the base addr.
    desc->base_bits31_to_24 = (0xFF000000 & base) >> 24;
    desc->base_bits23_to_16 = (0x00FF0000 & base) >> 16;
    desc->base_bits15_to_0  = (0x0000FFFF & base) >> 0;

    // Encode the limit.
    desc->limit_bits19_to_16 = (0xF0000 & size) >> 16;
    desc->limit_bits15_to_0 = (0x0FFFF & size) >> 0;

    // Those values will never change:
    desc->granularity = 1;     // 4KB units for the limit.
    desc->db = 1;              // 32-bit segment.
    desc->is64bits = 0;        // We are not in 64-bit mode.
    desc->avl = 0;             // Not used, set to 0.

    // All of our segments are ring 0.
    desc->priv = RING_0;

    // For now we only care about data or code segment, not system segments,
    // thus this bit is set to indicate that this is a code/data segment.
    desc->desc_type = 1;
    // Setup the access rights.
    desc->seg_type = type;

    // The segment is not fully setup, we can set the present bit.
    desc->present = 1;
}

// Refresh the segment registers and use the new code and data segments.
// After this function returns:
//      CS = `code_seg`
//      DS = SS = ES = FS = GS = `data_seg`.
static void
__refresh_segment_registers(uint16_t const code_seg, uint16_t const data_seg) {
    LOG("Use segments: Code = %x, Data = %x\n", code_seg, data_seg);
    write_cs(code_seg);
    write_ds(data_seg);
    write_ss(data_seg);
    write_es(data_seg);
    write_fs(data_seg);
    write_gs(data_seg);
}

void
gdt_init(gdt_t const gdt, size_t const gdt_size) {
    // We require at least 3 entries in the GDT: The null entry, the code
    // segment and the data segment.
    ASSERT(gdt_size>=3);
    // memzero the entire GDT.
    uint16_t const gdt_size_bytes = gdt_size * sizeof(struct segment_desc_t);
    memzero((uint8_t*)gdt, gdt_size_bytes);

    uint32_t const base = 0x0;
    uint32_t const limit = 0xFFFFF;
    uint8_t const code_seg_type = 0xA; // = CODE | EXECUTABLE.
    uint8_t const data_seg_type = 0x2; // = DATA | WRITABLE.

    // Create the flat code segment.
    gdt_add_segment(gdt, CODE_SEGMENT_IDX, base, limit, code_seg_type);
    // Create the flat data segment.
    gdt_add_segment(gdt, DATA_SEGMENT_IDX, base, limit, data_seg_type);

    // Generate the table descriptor for the GDT.
    struct table_desc_t table_desc = {
        .base_addr = (uint8_t*)gdt,
        // The limit should point to the last valid bytes, thus the -1.
        .limit = gdt_size_bytes-1,
    };
    LOG("GDT base address is %p, limit = %d bytes\n", gdt, gdt_size_bytes);

    // We can now load the GDT using the table_desc_t.
    LOG("Call lgdt with address %p\n", &table_desc);
    load_gdt(&table_desc);

    // Update segment registers.
    __refresh_segment_registers(CODE_SEGMENT_SELECTOR, DATA_SEGMENT_SELECTOR);
}
