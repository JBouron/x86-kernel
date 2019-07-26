// This frame allocator is used during early boot. It uses physical addressing
// and allocates the frames at the end of the kernel.
// Once paging is enabled and the kernel booted, a better fram allocator will be
// used, using virtual addressing this time.

#ifndef _ALLOC_EARLY_BOOT_H
#define _ALLOC_EARLY_BOOT_H

#include <includes/types.h>
#include <memory/paging/alloc/alloc.h>

// Init an early boot frame allocator.
void
fa_early_boot_init(struct early_boot_frame_alloc_t * const allocator);

// This is the allocating function. Append the frame after the kernel in RAM.
p_addr
fa_early_boot_alloc_frame(struct frame_alloc_t * const allocator);

#endif
