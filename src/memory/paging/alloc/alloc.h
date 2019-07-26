#ifndef _ALLOC_H
#define _ALLOC_H

// A frame allocator allocates physical page frames in memory. It is defined as
// a struct containing specific functions pointer carrying out the operations.
struct frame_alloc_t {
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
}__attribute__((packed));

// This is a "sub-class" for the frame allocator. This one is used for early
// boot and make sure to use physical addressing.
struct early_boot_frame_alloc_t {
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
    p_addr next_frame_addr;
}__attribute__((packed));

#endif
