#pragma once

// Declaration of the frame allocation structure.

#include <utils/types.h>

// A frame allocator is an entity implementing an algorithm for physical page
// frame allocation.
struct frame_alloc_t {
    // This is the allocation function. It it expected to return the physical
    // address of the physical page frame allocated.
    //   @param : A backpointer to the allocator itself.
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
}__attribute__((packed));
