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
    // Iterate over all addresses that are multiple of PAGE_SIZE.
    uint32_t const inc = PAGE_SIZE;
    for (v_addr v = dest, p = start; v < dest + size; v += inc, p += inc) {
        uint32_t const page_idx = v / PAGE_SIZE;
        uint32_t const pde_idx = page_idx / PAGEDIR_ENTRIES_COUNT;
        uint32_t const pte_idx = page_idx % PAGETABLE_ENTRIES_COUNT;

        // Check the existance of the PDE.
        struct pagedir_entry_t * const pde = root_page_dir + pde_idx;
        if (!pde->present) {
            // The PDE is not present, we need to create a new page table and
            // set this PDE.
            // Reset the PDE.
            memzero((uint8_t*)pde, sizeof(*pde));
            // Allocate new page table.
            pde->pagetable_addr = allocator->alloc_frame(allocator) >> 12;

            // Set some attributes.
            pde->writable = 1;
            pde->cacheable = 1;

            // Finally mark it as present.
            pde->present = 1;
        }

        pagetable_t pt = (pagetable_t) (pde->pagetable_addr << 12);
        struct pagetable_entry_t * const pte = pt + pte_idx;
        // Reset the PTE.
        memzero((uint8_t*)pte, sizeof(*pte));
        // Write PTE attributes.
        pte->writable = 1;
        pte->cacheable = 1;
        pte->pageframe_addr = p >> 12;

        // Set present bit.
        pte->present = 1;
    }
}

void
paging_init(void) {
}
