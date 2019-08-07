// This frame allocator is used during early boot. It uses physical addressing
// and allocates the frames at the end of the kernel.
// Once paging is enabled and the kernel booted, a better fram allocator will be
// used, using virtual addressing this time.

#ifndef _MEMORY_PAGING_ALLOC_EARLY_BOOT_H
#define _MEMORY_PAGING_ALLOC_EARLY_BOOT_H

#include <utils/types.h>
#include <memory/paging/alloc/alloc.h>

// This is a "sub-class" for the frame allocator. This one is used for early
// boot and make sure to use physical addressing.
struct early_boot_frame_alloc_t {
    p_addr (*alloc_frame)(struct frame_alloc_t * const);
    p_addr next_frame_addr;
}__attribute__((packed));

// Init an early boot frame allocator.
void
fa_early_boot_init(struct early_boot_frame_alloc_t * const allocator);

// This is the allocating function. Append the frame after the kernel in RAM.
p_addr
fa_early_boot_alloc_frame(struct frame_alloc_t * const allocator);

#endif
