#include <memory/paging/paging.h>
#include <includes/math.h>
#include <includes/kernel_map.h>
#include <memory/memory.h>

// The page directory used by the kernel. This table must be PAGE_SIZE bytes
// aligned.
pagedir_t KERNEL_PAGEDIRECTORY __attribute__((aligned(PAGE_SIZE)));

// This is the physical address of the next free frame in RAM.
static p_addr FRAME_ALLOC_ADDR;

// This function sets up the paging during early boot. It is defined as static
// so that it cannot be called anywhere else.
// This function assumes, when called, that addresses are virtual addresses and
// thus must convert them to physical ones by substracting the KERNEL_START
// address.
// We declare it in the .c file so that it is not accessible from anywhere else.
// The linker will find it though.
p_addr
__early_boot__setup_paging(void);

#define KERNEL_VIRT_START_ADDR  (0xC0000000)
#define PHYS_ADDR(addr) ((p_addr)(((uint8_t*)(addr)) - KERNEL_VIRT_START_ADDR))

// Allocate a new frame in RAM.
static p_addr
__new_frame(bool early_boot) {
    p_addr *frame_alloc_addr_ptr;
    if (early_boot) {
        frame_alloc_addr_ptr = (p_addr*)PHYS_ADDR(&FRAME_ALLOC_ADDR);
    } else {
        frame_alloc_addr_ptr = &FRAME_ALLOC_ADDR;
    }
    p_addr const addr = *frame_alloc_addr_ptr;
    *frame_alloc_addr_ptr += PAGE_SIZE;
    return addr;
}

static p_addr
__new_page_dir(bool early_boot) {
    // Allocate a new frame for the page dir and zero it.
    p_addr const pt_addr = __new_frame(early_boot);
    memzero((uint8_t*)pt_addr, PAGE_SIZE);
    return pt_addr;
}

static p_addr
__new_page_table(bool early_boot) {
    // Allocate a new frame for the page table and zero it.
    p_addr const pt_addr = __new_frame(early_boot);
    memzero((uint8_t*)pt_addr, PAGE_SIZE);
    return pt_addr;
}

// Map a physical memory region start:start+size to dest:dest+size. Addresses
// are assumed to be PAGE_SIZE aligned and size a multiple of PAGE_SIZE.
// root_page_dir is the address of the page directory to write the mapping to.
static void
__do_map(pagedir_t root_page_dir,
         bool early_boot,
         p_addr const start, 
         size_t const size,
         v_addr const dest) {
    // Iterate over all addresses that are multiple of PAGE_SIZE.
    for (v_addr v = dest, p = start; v < dest + size; v += PAGE_SIZE, p += PAGE_SIZE) {
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
            pde->pagetable_addr = __new_page_table(early_boot) >> 12;

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

p_addr
__early_boot__setup_paging(void) {
    // To setup paging we create two mappings of the kernel:
    //      _ An identity mapping, this is required since the next instruction
    //      fetched after enabling paging will still use physical addressing.
    //      _ A mapping in the higher half starting at KERNEL_VIRT_START_ADDR.

    // Compute the memory area occupied by the kernel. We want to map from
    // address KERNEL_START to KERNEL_END. 
    p_addr const start = PHYS_ADDR(KERNEL_START);
    p_addr const size = KERNEL_SIZE;

    // We need to setup the frame allocation address since we will need to
    // allocate the page directory and page tables.
    // the RAM region 0x00100000-0x00EFFFFF is free of use (and also contains
    // the kernel) we can start directly after the kernel sections.
    p_addr * frame_alloc_addr = (p_addr*)PHYS_ADDR(&FRAME_ALLOC_ADDR);
    // The address needs to be PAGE_SIZE aligned.
    *frame_alloc_addr = (1 + PHYS_ADDR(KERNEL_END) / PAGE_SIZE) * PAGE_SIZE;

    // Allocate the page directory.
    bool const early_boot = true;
    p_addr const kernel_page_dir = __new_page_dir(early_boot);

    // Create identity mapping.
    __do_map((pagedir_t)kernel_page_dir, early_boot, start, size, start);

    // Create higher-half mapping.
    v_addr const higher_half_start = KERNEL_VIRT_START_ADDR + start;
    __do_map((pagedir_t)kernel_page_dir, early_boot, start, size, higher_half_start);

    // Return address of the kernel page dir to be loaded in CR3.
    return kernel_page_dir;
}

void
paging_init(void) {
}

void
paging_map(p_addr const start, size_t const size, v_addr const dest) {
    ASSERT((start & (PAGE_SIZE-1)) == 0);
    ASSERT((dest & (PAGE_SIZE-1)) == 0);
    ASSERT((size & (PAGE_SIZE-1)) == 0);
    bool const early_boot = false;
    __do_map(KERNEL_PAGEDIRECTORY, early_boot, start, size, dest);
}
