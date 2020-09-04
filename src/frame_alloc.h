#pragma once
#include <types.h>
#include <multiboot.h>

// This file contains the public interface of the physical frame allocator used
// in this kernel.

// Initialize the frame allocator.
void init_frame_alloc(void);

// Special value returned by alloc_frame() to indicate that no physical frame
// could be allocated. We use this invalid value instead of NULL here so that
// the physical frame 0 can still be allocated/used without being mistaken for
// an error.
#define NO_FRAME    (void*)(-1UL)

// Allocate a new physical frame in RAM.
// @return: The physical address of the allocated physical frame. If no physical
// frame is available for allocation, this function returns NO_FRAME.
void *alloc_frame(void);

// Allocate a new physical frame in RAM under the 1MiB limit.
// @return: The physical address of the allocated physical frame. If no physical
// frame under 1MiB is available for allocation, this function returns
// NO_FRAME.
void *alloc_frame_low_mem(void);

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

// Set or Unset the Out Of Memory simulation.
// @param enabled: If true OOM simulation will be enabled and any subsequent
// call to frame_alloc() will return NO_FRAME. If false OOM is disabled and
// frame_alloc() will operate normally.
void frame_alloc_set_oom_simulation(bool const enabled);

// Tests related to the frame allocators.
void frame_alloc_test(void);
