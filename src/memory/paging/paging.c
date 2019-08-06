#include <memory/paging/paging.h>
#include <includes/math.h>
#include <memory/memory.h>

// The page directory used by the kernel. This table must be PAGE_SIZE bytes
// aligned.
pagedir_t KERNEL_PAGEDIRECTORY __attribute__((aligned(PAGE_SIZE)));

// Map a physical memory region start:start+size to dest:dest+size. Addresses
// are assumed to be PAGE_SIZE aligned and size a multiple of PAGE_SIZE.
// root_page_dir is the address of the page directory to write the mapping to.
void
paging_map(pagedir_t root_page_dir,
           struct frame_alloc_t *allocator,
           p_addr const start, 
           size_t const size,
           v_addr const dest) {
    ASSERT(0);
    ASSERT(root_page_dir);
    ASSERT(allocator);
    ASSERT(start);
    ASSERT(size);
    ASSERT(dest);
}

void
paging_init(void) {
}
