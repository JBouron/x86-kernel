#pragma once
#include <utils/types.h>

// This file contains types and functions to operate with the processor's GDT.

// +===========================================================================+
// | Types                                                                     |
// +===========================================================================+
// Enum describing the type of a segment.
enum segment_type_t {
    // Code segments are set to executable and read-only.
    SEGMENT_TYPE_CODE = 0xA,
    // Data segment are non-executable and writable.
    SEGMENT_TYPE_DATA = 0x2,
};

// Enum describing the privilege level required to access a segment.
enum segment_priv_level_t {
    SEGMENT_PRIV_LEVEL_RING0 = 0,
    SEGMENT_PRIV_LEVEL_RING3 = 3,
};

// The actual data structure representing a segment.
struct segment_desc_t {
    // The base linear address of the segment.
    l_addr base;
    // The size of the segment in pages. Valid values from 0 to 0xFFFFF.
    size_t size;
    // Type of segment.
    enum segment_type_t type;
    // The privilege level required for the segment.
    enum segment_priv_level_t priv_level;
};

// Wrapper describing a GDT.
struct gdt_t {
    // The number of entries in the GDT, including the NULL entry (which means
    // that this field should always be >0).
    size_t const size;
    // The actual array of entries. This is a void* to hide implementation
    // details. This is _not_ an array of segment_desc_t.
    void * const entries;
};

// Declare and define a new GDT variable with name `name` containing `size`
// entries.
// Declaring a GDT is a bit tricky since we can't rely on a dynamic memory
// allocator. Therefore this macro both declare the gdt struct and the static
// array that will contain the entries (under the name __<name>_data).
// This macro takes care of allocating the NULL entry, ie. the `size`
// is the number of entries in the GDT not counting the NULL entry.
#define DECLARE_GDT(name,_size)                 \
    uint8_t __##name_gdt_entries[(_size)*8];    \
    struct gdt_t name = {                       \
        .size = _size + 1,                      \
        .entries = (void *)__##name_gdt_entries,\
    };

// +===========================================================================+
// | Functions                                                                 |
// +===========================================================================+

// Initialize a GDT by zeroing the entries.
//   @param gdt: The gdt to be initialized.
void
gdt_init(struct gdt_t * const gdt);

// Add a segment to a GDT to a specific index.
//   @param gdt: The destination GDT.
//   @param index: The index of the entry to be populated. Note that this index
// has to be not used (eg. the present bit is set to 0).
//   @param desc: The segment descriptor describing the segment to be added.
// Once the segment is added to the GDT, its present bit is set.
void
gdt_add_segment(struct gdt_t * const gdt,
                uint16_t const index,
                struct segment_desc_t const *const desc);

// Load a GDT (using the lgdt instruction) to be used by the processor.
//   @param gdt: The GDT to be loaded.
// Note: This function does not change the segment registers, it is up to the
// programmer to change them after this function returns.
void
gdt_load(struct gdt_t const * const gdt);

// Read a segment descriptor from a GDT.
//   @param gdt: The GDT to read from.
//   @param index: The index in the GDT of the entry to be returned.
//   @param out_desc: Pointer to the struct segment_desc_t to be read into.
void
gdt_get_segment(struct gdt_t const * const gdt,
                uint16_t const index,
                struct segment_desc_t * const out_desc);

// Remove an entry from a GDT.
//   @param gdt: The GDT to remove the entry from.
//   @param index: The index of the entry to remove.
// Note: The removal of the entry means zeroing the entire entry, not just
// clearing the present bit.
void
gdt_remove_segment(struct gdt_t * const gdt, uint16_t const index);

// Test wether a segment is marke as present in a GDT.
//   @param gdt: The GDT to read from.
//   @param index: The index of the segment for which the present test should be
//   carried out.
// @return: true if the segment is present, false otherwise.
bool
gdt_segment_is_present(struct gdt_t const * const gdt, uint16_t const index);
