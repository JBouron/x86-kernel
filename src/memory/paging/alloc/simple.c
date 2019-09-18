#include <memory/paging/alloc/simple.h>
#include <memory/paging/paging.h>
#include <utils/memory.h>

// The code is very similar to the early boot allocator since they both use the
// same algorithm.

#define PAGE_SIZE (4096)

void
fa_simple_alloc_init(struct simple_frame_alloc_t * const allocator,
                     p_addr const next_frame_addr) {
    // Setup the function pointer to the allocating function.
    allocator->super.alloc_frame = (p_addr (*)(struct frame_alloc_t *const))
        (fa_simple_alloc_frame);

    // The memory region between KERNEL_END -> 0x00E00000 if free for use (as
    // long as it exists). See https://wiki.osdev.org/Memory_Map_(x86).
    // For now 14 MiB should be enough for our purpose.
    allocator->max_frame_addr = 0x00E00000;

    // The early boot allocator allocated a page directory for the kernel as
    // well as a couple of page tables. Take a simplistic approach by starting
    // where the early boot allocator left off.
    allocator->next_frame_addr = next_frame_addr;
}

#define SAF(allocator) ((struct simple_frame_alloc_t*)(allocator))
p_addr
fa_simple_alloc_frame(struct frame_alloc_t * const allocator) {
    struct simple_frame_alloc_t * const s_allocator = SAF(allocator);

    // The allocated frame is the next available frame.
    p_addr const addr = s_allocator->next_frame_addr;

    // Increase the pointer to the next frame.
    s_allocator->next_frame_addr += PAGE_SIZE;

    // Check that we have not reached the end.
    ASSERT(addr < s_allocator->max_frame_addr);

    // Return the address of the allocated frame.
    return addr;
}
#undef SAF
