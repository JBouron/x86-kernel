#ifndef _MEMORY_PAGING_ALLOC_ALLOC_H
#define _MEMORY_PAGING_ALLOC_ALLOC_H

#include <utils/types.h>

// A frame allocator allocates physical page frames in memory. It is defined as
// a struct containing specific functions pointer carrying out the operations.
struct frame_alloc_t {
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
}__attribute__((packed));

// This is the main frame allocator used throughout the kernel after paging is
// setup.
extern struct frame_alloc_t FRAME_ALLOCATOR;

// A shortcut function to allocate a frame.
p_addr
allocate_frame(void);
#endif
