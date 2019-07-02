// Define types and helpers for memory segmentation.
#ifndef _SEGMENT_H
#define _SEGMENT_H
#include <includes/types.h>

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

// When creating a new GDT or IDT, the processor expects to get the base address
// and the size (in bytes) of the table. A table descriptor is a 6-byte value
// containing both of these values.
// Upon calling lgdt or lidt, an address to a table_desc_t is passed.
struct table_desc_t {
    // The size in bytes of the table.
    uint16_t limit;
    // The base address of the table.
    uint8_t *base_addr;
} __attribute__((packed));
#endif
