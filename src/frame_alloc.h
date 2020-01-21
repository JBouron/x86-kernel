#pragma once
#include <types.h>
#include <multiboot.h>

// This file contains the public interface of the physical frame allocator used
// in this kernel.

// Initialize the frame allocator.
void init_frame_alloc(void);

// Allocate a new physical frame in RAM.
// @return: The physical address of the allocated physical frame.
void *alloc_frame(void);

// Free up a physical frame.
// @param ptr: The poitner to the frame to be freed up. Note this pointer must
// be 4KiB aligned.
void free_frame(void const * const ptr);

// Get the number of physical frames currently allocated.
// @return: The number of frames currently allocated.
uint32_t frames_allocated(void);

// Indicate to the frame allocator that paging has been enabled and that it
// should use virtual pointers to higher half memory internally, note that the
// allocator will _still_ return physical addresses in alloc_frame.
// Note: This is needed as the frame allocator is initialized before paging is
// enabled, therefore, at first, it uses physical addresses internally (that is
// within its data structure(s)).
void fixup_frame_alloc_to_virt(void);

// Tests related to the frame allocators.
void frame_alloc_test(void);
