// A simple frame allocator.
// This allocator allocates the frames after the kernel (as in the early boot
// allocator).

#ifndef _ALLOC_SIMPLE_H
#define _ALLOC_SIMPLE_H

#include <utils/types.h>
#include <memory/paging/alloc/alloc.h>

// "Subclass" of the frame_alloc_t structure use by the simple frame allocator.
struct simple_frame_alloc_t {
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
    // The address of the next free frame.
    p_addr next_frame_addr;
    // The address of the limit for frame allocation. It is never ok to allocate
    // *after* this limit.
    p_addr max_frame_addr;
}__attribute__((packed));

// Initialize a simple frame allocator using the specified next_frame_addr.
void
fa_simple_alloc_init(struct simple_frame_alloc_t * const allocator,
                     p_addr const next_frame_addr);

// This is the allocating function. Append the frame after the kernel in RAM.
p_addr
fa_simple_alloc_frame(struct frame_alloc_t * const allocator);

#endif
