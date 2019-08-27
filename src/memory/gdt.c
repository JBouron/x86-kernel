#include <memory/gdt.h>
#include <memory/segment.h>
#include <asm/asm.h>
#include <utils/memory.h>
#include <utils/debug.h>

// This kernel uses a flat model, that is we are using only two segement (one
// data segment and one code segment) both covering the entire linear address
// space.

// Segment descriptors are 8-bytes value containing, among other things, the
// base address and the limit for a segment.
// This struct encodes this 8-byte value in a more readable way.
struct segment_desc_t {
    // Lower 16-bits of the limit.
    uint16_t limit_bits15_to_0 : 16;
    // Some bits of the base.
    uint16_t base_bits15_to_0 : 16;
    uint8_t base_bits23_to_16 : 8;
    // Indicate the type of segment encoded in 4 bits as follows:
    // MSB: if 1, this is a code segment, else a data segment.
    // If this is a code segment the last 3 bits are: C, R and A, where C
    // indicate if the segment is conforming, R if it is readable and A
    // accessed.
    // If this is a data segment, the last 3 bits are: E, W and A where E
    // indicates "Expand-down", W writable and A accessed.
    uint8_t seg_type : 4;
    // 0 = system segment, 1 = code or data segment.
    uint8_t desc_type : 1;
    // The priviledge level/ring required to use the segment.
    uint8_t priv : 2;
    // Is the segment present ?
    uint8_t present : 1;
    // Some bits of the limit.
    uint8_t limit_bits19_to_16 : 4;
    // Available bit. Unused for now.
    uint8_t avl : 1;
    // Should always be 0. (Reserved in non-IAe mode).
    uint8_t is64bits : 1;
    // Operation size: 0 = 16-bit, 1 = 32 bit segment. Should always be 1.
    uint8_t db : 1;
    // Scaling of the limit field. If 1 limit is interpreted in 4KB units,
    // otherwise in bytes units.
    uint8_t granularity : 1;
    // Some bits of the base.
    uint8_t base_bits31_to_24 : 8;
} __attribute__((packed));

// The code segment of the kernel is at index 1 in the GDT while the data
// segment is at index 2.
uint16_t const KERNEL_CODE_SEGMENT_IDX = 1;
uint16_t const KERNEL_DATA_SEGMENT_IDX = 2;

uint16_t const KERNEL_CODE_SEGMENT_SELECTOR =
    KERNEL_CODE_SEGMENT_IDX << 3 | RING_0;

uint16_t const KERNEL_DATA_SEGMENT_SELECTOR =
    KERNEL_DATA_SEGMENT_IDX << 3 | RING_0;

// This is the GDT, it only contains 3 entries:
//  _ The NULL entry, it is mandatory,
//  _ One code segment spanning the entire address space.
//  _ One data segment spanning the entire address space.
#define GDT_SIZE (3)
static struct segment_desc_t GDT[GDT_SIZE];

// Add a segment to the GDT.
static void
__add_segment(uint16_t const index,
              uint32_t const base,
              uint32_t const size,
              uint8_t const type) {
    // The size of the segment must be 4KB granularity so that we can cover the
    // entire 32-bit addressable space.
    ASSERT(0 < index && index < 8192);
    struct segment_desc_t * const desc = GDT + index;

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
gdt_init(void) {
    // memzero the entire GDT.
    uint16_t const gdt_size_bytes = GDT_SIZE * sizeof(struct segment_desc_t);
    memzero((uint8_t*)GDT, gdt_size_bytes);

    uint32_t const base = 0x0;
    uint32_t const limit = 0xFFFFF;
    uint8_t const code_seg_type = 0xA; // = CODE | EXECUTABLE.
    uint8_t const data_seg_type = 0x2; // = DATA | WRITABLE.

    // Create the flat code segment.
    __add_segment(KERNEL_CODE_SEGMENT_IDX, base, limit, code_seg_type);
    // Create the flat data segment.
    __add_segment(KERNEL_DATA_SEGMENT_IDX, base, limit, data_seg_type);

    // Generate the table descriptor for the GDT.
    struct table_desc_t table_desc = {
        .base_addr = (uint8_t*)GDT,
        // The limit should point to the last valid bytes, thus the -1.
        .limit = gdt_size_bytes-1,
    };
    LOG("GDT base address is %p, limit = %d bytes\n", GDT, gdt_size_bytes);

    // We can now load the GDT using the table_desc_t.
    LOG("Call lgdt with address %p\n", &table_desc);
    load_gdt(&table_desc);

    // Update segment registers.
    __refresh_segment_registers(KERNEL_CODE_SEGMENT_SELECTOR,
                                KERNEL_DATA_SEGMENT_SELECTOR);
}
