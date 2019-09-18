#include <memory/gdt/gdt.h>
#include <memory/segment.h>
#include <asm/asm.h>
#include <utils/memory.h>
#include <utils/debug.h>

// +===========================================================================+
// | Static functions, types and consts                                        |
// +===========================================================================+

// Note the GDT implementation:
// In the type struct gdt the field `entries` is of type void* to hide
// implementation details.
// The real type of this field is struct __x86_segment_desc *.
// However, we do not want to expose the real format of the entries for multiple
// reasons:
//      _ First it would mean that the programmer can directly modify the
//      entries without using the public interface of the GDT module. While it
//      is still possible to fiddle with them directly, using the void* makes it
//      harder.
//      _ Second, the struct __x86_segment_desc is a mess, a lot of fields
//      have fixed values or have their bits scatterred. We chose to use a
//      simplified format for the segment descriptors in the public interface:
//      struct segment_desc.

// The GDT can have at most 8192 entries.
#define MAX_GDT_INDEX   (8192)

// This is the actual x86 layout of a segment descriptor.
struct __x86_segment_desc {
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
//   @param from: The simplified segment descriptor to be translated.
//   @param to: Pointer to the real x86 segment descriptor to be written to.
static void
__translate_segment_desc(struct segment_desc const * const from,
                         struct __x86_segment_desc * const to) {
    // Encode the base addr.
    uint32_t const base = from->base;
    to->base_bits31_to_24 = (0xFF000000 & base) >> 24;
    to->base_bits23_to_16 = (0x00FF0000 & base) >> 16;
    to->base_bits15_to_0  = (0x0000FFFF & base) >> 0;

    // Those values will never change:
    to->db = 1;              // 32-bit segment.
    to->is64bits = 0;        // We are not in 64-bit mode.
    to->avl = 0;             // Not used, set to 0.
    to->granularity = 1;     // 4KB units for the limit.

    // Encode the limit.
    uint32_t const limit = from->size;
    ASSERT(limit <= 0xFFFFF);
    to->limit_bits19_to_16 = (0xF0000 & limit) >> 16;
    to->limit_bits15_to_0 = (0x0FFFF & limit) >> 0;

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
// Get a pointer to an entry in the GDT.
//   @param gdt: The GDT from which to get the address of the entry.
//   @param index: The index of the entry of interest.
static struct __x86_segment_desc *
__get_x86_entry_ptr(struct gdt const * const gdt, uint16_t const index) {
    struct __x86_segment_desc *d = (struct __x86_segment_desc*)gdt->entries;
    return d + index;
}

// Compute the size in bytes of a GDT.
//   @param gdt: The GDT.
static size_t
__gdt_size_bytes(struct gdt const * const gdt) {
    return gdt->size * sizeof(struct __x86_segment_desc);
}

// Check that an index is within the bounds of a GDT. Panic if this is not the
// case.
//   @param gdt: The GDT on which to perform the test.
//   @param index: The index.
// A valid index is an index that is < gdt->size. 0 is considered within the
// bounds.
static void
__check_bounds(struct gdt const * const gdt, uint16_t const index) {
    ASSERT(index < MAX_GDT_INDEX && index < gdt->size);
}

// +===========================================================================+
// | Public interface implementation                                           |
// +===========================================================================+

void
gdt_init(struct gdt * const gdt) {
    // memzero the entire GDT. This will take care of setting up the NULL entry.
    ASSERT(gdt->size);
    size_t const gdt_size_bytes = __gdt_size_bytes(gdt);
    memzero((uint8_t*)gdt->entries, gdt_size_bytes);
}

void
gdt_add_segment(struct gdt * const gdt,
                uint16_t const index,
                struct segment_desc const * const desc) {
    __check_bounds(gdt, index);
    // The first entry is the NULL entry and thus cannot be modified.
    ASSERT(0 < index);

    struct __x86_segment_desc * x86d = __get_x86_entry_ptr(gdt, index);

    // The current entry in the GDT should not be present. If the entry is
    // present then it might be currently used.
    ASSERT(!x86d->present);

    // Fill up the descriptor with the info passed through the simplified
    // descriptor.
    __translate_segment_desc(desc, x86d);
}

// Forward declaration of the lgdt x86 instruction wrapper loading a GDT table
// from a table descriptor.  The actual implementation of this function can be
// found in gdt_asm.S.
void
__x86_lgdt(struct table_desc const * const desc);

void
gdt_load(struct gdt const * const gdt) {
    ASSERT(gdt->size);
    size_t const gdt_size_bytes = __gdt_size_bytes(gdt);

    // Generate the table descriptor for the GDT. Note that the limit field
    // should be the size of the table -1, since base_addr + limit is supposed
    // to point on the last byte of the table.
    struct table_desc table_desc = {
        .base_addr = (uint8_t*)gdt->entries,
        .limit = gdt_size_bytes-1,
    };

    // Use the table descriptor to load the GDT.
    __x86_lgdt(&table_desc);
    // At this point the processor has loaded the new GDT, but the old segment
    // registers (shadow registers) are still in use.
}

void
gdt_get_segment(struct gdt const * const gdt,
                uint16_t const index,
                struct segment_desc * const out_desc) {
    __check_bounds(gdt, index);

    // Get a pointer on the segment selector.
    struct __x86_segment_desc const * const entry =
        __get_x86_entry_ptr(gdt, index);

    // Parse the base address from the multiple parts.
    out_desc->base = entry->base_bits15_to_0 |
        (entry->base_bits23_to_16 << 6) | (entry->base_bits31_to_24 << 24);

    // Parse size.
    out_desc->size = entry->limit_bits15_to_0 |
        (entry->limit_bits19_to_16 << 16);

    // The segment type and privilge level are encoded as is.
    out_desc->type = entry->seg_type;
    out_desc->priv_level = entry->priv;
}

void
gdt_remove_segment(struct gdt * const gdt, uint16_t const index) {
    __check_bounds(gdt, index);
    ASSERT(0 < index);

    // Get a pointer on the entry in the actual GDT and zero it. This clears the
    // present bit and therefore removes the segment from the GDT.
    struct __x86_segment_desc * d = __get_x86_entry_ptr(gdt, index);
    memzero(d, sizeof(*d));
}

bool
gdt_segment_is_present(struct gdt const * const gdt, uint16_t const index) {
    __check_bounds(gdt, index);

    if (!index) {
        // The NULL segment is always present, even if its present bit tells
        // otherwise.
        return true;
    } else {
        struct __x86_segment_desc const * const d =
            __get_x86_entry_ptr(gdt, index);
        return d->present;
    }
}
