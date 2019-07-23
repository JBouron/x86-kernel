#include <memory/paging/paging.h>
#include <includes/math.h>
#include <includes/kernel_map.h>

// The page directory used by the kernel. This table must be PAGE_SIZE bytes
// aligned.
pagedir_t KERNEL_PAGEDIRECTORY __attribute__((aligned(PAGE_SIZE)));

// This function sets up the paging during early boot. It is defined as static
// so that it cannot be called anywhere else.
// This function assumes, when called, that addresses are virtual addresses and
// thus must convert them to physical ones by substracting the KERNEL_START
// address.
// We declare it in the .c file so that it is not accessible from anywhere else.
// The linker will find it though.
uint32_t
__early_boot__setup_paging(void);

static void
__early_boot__add_pde(pagedir_t pd,
                      uint16_t const idx,
                      uint32_t const table_offset) {
    struct pagedir_entry_t *const pde = pd + idx;
    pde->present = 1;
    pde->writable = 1;
    pde->user_accessible = 0;
    pde->write_through = 0;
    pde->cacheable = 1;
    pde->huge_page = 0;
    pde->pagetable_addr = ((uint32_t)table_offset) >> 12;
}

static void
__early_boot__add_pte(pagetable_t pt,
                      uint16_t const idx,
                      uint32_t const page_offset) {
    ASSERT(!(page_offset & 0xfff));
    // Page offset must be 4KiB aligned.
    struct pagetable_entry_t * const entry = pt + idx;
    entry->present = 1;
    entry->writable = 1;
    entry->user_accessible = 0;
    entry->write_through = 0;
    entry->cacheable = 1;
    entry->zero = entry->zero2 = 0;
    entry->pageframe_addr = page_offset >> 12;
}

uint32_t
__early_boot__setup_paging(void) {
#define KERNEL_VIRT_START_ADDR  (0xC0000000)
#define PHYS_ADDR(addr) (((uint8_t*)(addr)) - KERNEL_VIRT_START_ADDR)
#define VIRT_ADDR(addr) (((uint8_t*)(addr)) + KERNEL_VIRT_START_ADDR)
    // To setup paging we need both an identity mapping of the first N Mbytes
    // (the size of the entire kernel in MB) and a mapping at the virtual
    // address of the kernel to the first N MB in physical memory.

    uint8_t const * const kernel_end = PHYS_ADDR(KERNEL_END);

    // Compute the number of pages and page tables we need to allocate.
    uint32_t const num_alloc_pages = ceil_x_over_y((uint32_t)kernel_end,
                                                   PAGE_SIZE);

    // The start alloc addr indicates the physical address at which we start
    // allocating frames. Since here the kernel's frame are already allocated
    // (eg. they are in physical memory already), we only need to allocate the
    // page directory and the page table(s).
    uint32_t const start_alloc_addr = (1 + (uint32_t)(kernel_end) / PAGE_SIZE)
        * PAGE_SIZE;
    // This is the next available address for allocation.
    uint32_t alloc_addr = start_alloc_addr;

    // The first page in the pool is used for the kernel's page directory.
    pagedir_t kernel_page_dir = (pagedir_t)alloc_addr;
    alloc_addr += PAGE_SIZE;

    // We need at least one page table.
    pagetable_t curr_pagetable = NULL;

    for(uint32_t pageidx = 0; pageidx < num_alloc_pages; ++pageidx) {
        uint32_t const page_start_addr = pageidx * PAGE_SIZE;
        uint16_t const pte_idx = pageidx % 1024;
        if (!pte_idx) {
            // We reached the end of the current page table, allocate a new one
            // an continue the allocation with it.
            curr_pagetable = (pagetable_t)alloc_addr;
            alloc_addr += PAGE_SIZE;

            uint16_t const pde_idx = pageidx / 1024;
            __early_boot__add_pde(kernel_page_dir,
                                  pde_idx, 
                                  (uint32_t)curr_pagetable);
        }
        __early_boot__add_pte(curr_pagetable, pte_idx, page_start_addr);
    }

    // We now have to map the kernel to the higher-half.
    uint32_t const start_kernel_page = KERNEL_VIRT_START_ADDR / PAGE_SIZE;
    uint32_t const start_kernel_pde_idx = start_kernel_page / 1024;
    // Simply copy the PDEs from the identity mapping to the higher half.
    uint32_t const num_pdes = ceil_x_over_y(num_alloc_pages, 1024);
    for (uint32_t i = 0; i < num_pdes; ++i) {
        kernel_page_dir[i+start_kernel_pde_idx] = kernel_page_dir[i];
    }

    // Set the KERNEL_PAGEDIRECTORY global variable. It should be set to the
    // virtual address of `kernel_page_dir` since this global variable will be
    // used *after* paging is enabled.
    pagedir_t *const global = (pagedir_t*)PHYS_ADDR(&KERNEL_PAGEDIRECTORY);
    *global = (pagedir_t)VIRT_ADDR(kernel_page_dir);

    // Return the physical address of the page directory that must be used in
    // CR3.
    return (uint32_t) kernel_page_dir;
    
#undef PHYS_ADDR
#undef KERNEL_START
#undef KERNEL_VIRT_START_ADDR
}

void
paging_init(void) {
}
