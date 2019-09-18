#pragma once

#include <utils/types.h>

// A frame allocator allocates physical page frames in memory. It is defined as
// a struct containing specific functions pointer carrying out the operations.
struct frame_alloc_t {
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
    p_addr dummy1;
    p_addr dummy2;
}__attribute__((packed));

// This is the main frame allocator used throughout the kernel after paging is
// setup.
extern struct frame_alloc_t FRAME_ALLOCATOR;

// A shortcut function to allocate a frame.
p_addr
allocate_frame(void);
