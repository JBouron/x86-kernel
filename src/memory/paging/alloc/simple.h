// A simple frame allocator.
// This allocator allocates the frames after the kernel (as in the early boot
// allocator).

#pragma once

#include <utils/types.h>
#include <memory/paging/alloc/alloc.h>

// "Substruct" of the frame_alloc_t structure.
struct simple_frame_alloc {
    // This should be the first field so that we can easily convert this
    // allocator to a struct frame_alloc.
    struct frame_alloc super;
    // The address of the next free frame.
    p_addr next_frame_addr;
    // The address of the limit for frame allocation. It is never ok to allocate
    // *after* this limit.
    p_addr max_frame_addr;
}__attribute__((packed));

// Initialize a simple frame allocator.
//   @param allocator: Pointer to the simple frame allocator to be initialized.
//   @parma next_frame_addr: The smallest physical address available for
//   allocation.
void
fa_simple_alloc_init(struct simple_frame_alloc * const allocator,
                     p_addr const next_frame_addr);

// Allocate a new physical page frame after the kernel.
//   @param allocator: Backpointer to the simple_frame_alloc_t struct.
// @return: The physical address of the page.
// Note: The page is zero'ed upon allocation.
p_addr
fa_simple_alloc_frame(struct frame_alloc * const allocator);

