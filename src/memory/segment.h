// Define types and helpers for memory segmentation.
#ifndef _SEGMENT_H
#define _SEGMENT_H
#include <includes/types.h>

// Privilege levels.
#define RING_0  (0)
#define RING_3  (3)

// We use specific segments for the kernel.
extern uint16_t const KERNEL_CODE_SEGMENT_IDX;
extern uint16_t const KERNEL_DATA_SEGMENT_IDX;
extern uint16_t const KERNEL_CODE_SEGMENT_SELECTOR;
extern uint16_t const KERNEL_DATA_SEGMENT_SELECTOR;

// When creating a new GDT or IDT, the processor expects to get the base address
// and the size (in bytes) of the table. A table descriptor is a 6-byte value
// containing both of these values.
// Upon calling lgdt or lidt, an address to a table_desc_t is passed.
struct table_desc_t {
    // The size in bytes of the table. Note that base_addr + limit should point
    // to the last valid bytes, thus in practice, limit = size - 1.
    uint16_t limit;
    // The base address of the table.
    uint8_t *base_addr;
} __attribute__((packed));
#endif
