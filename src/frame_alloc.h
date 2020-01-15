#pragma once

// This file contains the public interface of the physical frame allocator used
// in this kernel.

// Initialize the frame allocator.
void init_frame_alloc(void);

// Allocate a new physical frame in RAM.
// @return: The physical address of the allocated physical frame.
void *alloc_frame(void);

// Indicate to the frame allocator that paging has been enabled and that it
// should use virtual pointers to higher half memory internally, note that the
// allocator will _still_ return physical addresses in alloc_frame.
// Note: This is needed as the frame allocator is initialized before paging is
// enabled, therefore, at first, it uses physical addresses internally (that is
// within its data structure(s)).
void fixup_frame_alloc_to_virt(void);

// Tests related to the frame allocators.
void frame_alloc_test(void);
