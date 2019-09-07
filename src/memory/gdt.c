#include <memory/gdt.h>
#include <memory/segment.h>
#include <asm/asm.h>
#include <utils/memory.h>
#include <utils/debug.h>

// This is the actual x86 layout of a segment descriptor.
struct __x86_segment_desc_t {
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

// Convert a segment_desc_t to the actual x86 segment descriptor.
static void
__translate_segment_desc(struct segment_desc_t const * const from,
                         struct __x86_segment_desc_t * const to) {
    // Encode the base addr.
    uint32_t const base = from->base;
    to->base_bits31_to_24 = (0xFF000000 & base) >> 24;
    to->base_bits23_to_16 = (0x00FF0000 & base) >> 16;
    to->base_bits15_to_0  = (0x0000FFFF & base) >> 0;

    // Encode the limit.
    uint32_t const size = from->size;
    to->limit_bits19_to_16 = (0xF0000 & size) >> 16;
    to->limit_bits15_to_0 = (0x0FFFF & size) >> 0;

    // Those values will never change:
    to->granularity = 1;     // 4KB units for the limit.
    to->db = 1;              // 32-bit segment.
    to->is64bits = 0;        // We are not in 64-bit mode.
    to->avl = 0;             // Not used, set to 0.

    to->priv = (uint8_t)from->priv_level;

    // For now we only care about data or code segment, not system segments,
    // thus this bit is set to indicate that this is a code/data segment.
    to->desc_type = 1;
    // Setup the access rights.
    to->seg_type = from->type;

    // The segment is not fully setup, we can set the present bit.
    to->present = 1;
}

// Get a pointer to the x86 segment descriptor at a specified index in the GDT.
static struct __x86_segment_desc_t *
__get_x86_segment_desc_ptr(struct gdt_t const * const gdt, uint16_t index) {
    struct __x86_segment_desc_t *d = (struct __x86_segment_desc_t*)gdt->entries;
    return d + index;
}

// Add a segment to the GDT.
void
gdt_add_segment(struct gdt_t * const gdt,
                uint16_t const index,
                struct segment_desc_t const desc) {
    // The size of the segment must be 4KB granularity so that we can cover the
    // entire 32-bit addressable space.
    ASSERT(0 < index && index < 8192);
    ASSERT(index < gdt->size);

    // Get a ptr on the segment to fill up.
    struct __x86_segment_desc_t * x86d = __get_x86_segment_desc_ptr(gdt, index);

    // The current entry in the GDT should not be present. We only support
    // addition for the moment.
    ASSERT(!x86d->present);

    // Fill up the descriptor with the info passed through the simplified
    // descriptor.
    __translate_segment_desc(&desc, x86d);
}

// Compute the size in bytes of a GDT.
static size_t
__gdt_size_bytes(struct gdt_t const * const gdt) {
    return gdt->size * sizeof(struct __x86_segment_desc_t);
}

void
gdt_init(struct gdt_t * const gdt) {
    // memzero the entire GDT. This will take care of setting up the NULL entry.
    size_t const gdt_size_bytes = __gdt_size_bytes(gdt);
    memzero((uint8_t*)gdt->entries, gdt_size_bytes);
}

// Prototype of the lgdt x86 instruction wrapper loading a GDT table from a
// table descriptor.
void __x86_lgdt(struct table_desc_t const * const desc);

void
gdt_load(struct gdt_t const * const gdt) {
    size_t const gdt_size_bytes = __gdt_size_bytes(gdt);
    // Generate the table descriptor for the GDT.
    struct table_desc_t table_desc = {
        .base_addr = (uint8_t*)gdt->entries,
        // The limit should point to the last valid byte of the GDT thus the -1.
        .limit = gdt_size_bytes-1,
    };
    LOG("GDT base address is %p, limit = %d bytes\n", gdt, gdt_size_bytes);

    // We can now load the GDT using the table_desc_t.
    LOG("Call lgdt with address %p\n", &table_desc);
    __x86_lgdt(&table_desc);
}
