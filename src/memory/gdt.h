// GDT operations.
#ifndef _MEMORY_GDT_H
#define _MEMORY_GDT_H
#include <utils/types.h>

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

// A Global Descriptor Table is no more no less than an address in the linear
// address space.
typedef struct segment_desc_t * gdt_t;

void
gdt_add_segment(gdt_t const gdt,
                uint16_t const index,
                uint32_t const base,
                uint32_t const size,
                uint8_t const type);

// Initialize a GDT to get a flat model with one code segment and one data
// segment covering the entire address space.
// This function also refreshes all segment register to use the new segments,
// therefore the new segments come in effect right after this function returns.
// `gdt_size` refers to the number of entries available in the provided GDT.
// This is accounting for the NULL entry (mandatory).
void
gdt_init(gdt_t const gdt, size_t const gdt_size);
#endif
