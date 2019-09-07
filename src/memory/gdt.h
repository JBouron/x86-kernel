#ifndef _MEMORY_GDT_H
#define _MEMORY_GDT_H
#include <utils/types.h>

// This file contains types and functions to operate with the processor's GDT.

// Declare and define a new GDT variable with name `name` containing `size`
// entries. This macro takes care of allocating the NULL entry, ie. the `size`
// is the number of entries in the GDT not counting the NULL entry.
// Note that this macro will also define the underlying memory region that will
// contain the GDT entries under the name __<name>_data.
#define DECLARE_GDT(name,_size)                 \
    uint8_t __##name_gdt_entries[(_size)*8];    \
    struct gdt_t name = {                       \
        .size = _size + 1,                      \
        .entries = (void *)__##name_gdt_entries,\
    };

// Enum for the segment type, either code or data. Code segments are set to
// executable and data segments redable.
enum segment_type_t {
    SEGMENT_TYPE_CODE = 0xA,
    SEGMENT_TYPE_DATA = 0x2,
};

// The priviledge level indicates what level is required to access the segment.
enum segment_priv_level_t {
    SEGMENT_PRIV_LEVEL_RING0 = 0,
    SEGMENT_PRIV_LEVEL_RING3 = 3,
};

// This structure describes a segment.
struct segment_desc_t {
    // The base virtual address of the segment.
    v_addr base;
    // The size of the segment, this is by increments of 4KB.
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
    // details.
    void * const entries;
};

// Add a segment to a GDT to a specific index.
void
gdt_add_segment(struct gdt_t * const gdt,
                uint16_t const index,
                struct segment_desc_t const desc);

// Initialize a GDT by zeroing the table.
void
gdt_init(struct gdt_t * const gdt);

// Load a GDT on the processor.
// Note: This function does not change the segment register, it is up to the
// programmer to change them after this function returns.
void
gdt_load(struct gdt_t const * const gdt);
#endif
