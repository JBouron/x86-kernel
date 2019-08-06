#include <memory/paging/alloc/alloc.h>

// This is the main frame allocator used throughout the kernel after paging is
// setup.
struct frame_alloc_t FRAME_ALLOCATOR;

p_addr
allocate_frame(void) {
    struct frame_alloc_t * allocator = (struct frame_alloc_t *)&FRAME_ALLOCATOR;
    return allocator->alloc_frame(allocator);
}
