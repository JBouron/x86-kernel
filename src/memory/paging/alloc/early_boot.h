// This frame allocator is used during early boot. It uses physical addressing
// and allocates the frames at the end of the kernel.  Once paging is enabled
// and the kernel booted, a better frame allocator will be used, using virtual
// addressing this time.

#pragma once

#include <utils/types.h>
#include <memory/paging/alloc/alloc.h>

// This is a "sub-class" for the frame allocator. This one is used for early
// boot and make sure to use physical addressing.
struct early_boot_frame_alloc {
    // This should be the first field so that we can easily convert this
    // allocator back to a struct frame_alloc.
    struct frame_alloc super;
    // The address of the next free frame.
    p_addr next_frame_addr;
}__attribute__((packed));

// Initialize an early boot frame allocator.
//   @param allocator: Pointer to the early boot frame allocator to be
//   initialized.
void
fa_early_boot_init(struct early_boot_frame_alloc * const allocator);

// Allocate a new physical page frame after the kernel.
//   @param allocator: Backpointer to the early_boot_frame_alloc_t struct.
// @return: The physical address of the page.
p_addr
fa_early_boot_alloc_frame(struct frame_alloc * const allocator);

