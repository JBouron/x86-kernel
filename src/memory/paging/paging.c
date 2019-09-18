#include <memory/paging/paging.h>
#include <utils/math.h>
#include <utils/memory.h>
#include <utils/kernel_map.h>

// The page directory used by the kernel. This table must be PAGE_SIZE bytes
// aligned.
struct vm KERNEL_PAGEDIRECTORY;

// Kernel wide physical page frame allocator.
struct frame_alloc *FRAME_ALLOCATOR;

// Get the physical address of the page directory currently in use (ie. loaded
// in cr3).
//   @return The physical address.
static p_addr
__get_curr_pagedir_addr(void) {
    uint32_t const cr3 = read_cr3();
    return (p_addr)cr3;
}

// Get a pointer to the page directory entry at index `index`.
static struct pagedir_entry *
__get_pde(struct vm * const vm, uint16_t const index) {
    ASSERT(index < PAGEDIR_ENTRIES_COUNT);
    if (paging_enabled()) {
        // TODO: The following code assumes that `vm` is the current page
        // directory in use.
        ASSERT(__get_curr_pagedir_addr() == vm->pagedir_addr);

        // We use the recursive entry of the page_directory. We want to use the
        // page dir as a regular page. Therefore, the pde index and pte index of
        // the constructed virtual address should both be 1024, thus the address
        // starts with 0xFFFFF000.  Now, since we want the pde at index `index`,
        // the offset is index * the size of a pde. We add this to the base and
        // we end up with the constructed address as computed below.
        v_addr const page_dir_addr = 0xFFFFF000; 
        v_addr const addr = page_dir_addr + index * sizeof(struct pagedir_entry);
        return (struct pagedir_entry *) addr;
    } else {
        // We are still in early boot, paging is disabled thus we can directly
        // read from the struct.
        struct pagedir * const pd = (struct pagedir*) vm->pagedir_addr;
        return pd->entry + index;
    }
}

// Get a pointer to a PTE.
static struct pagetable_entry *
__get_pte(struct vm * const vm, uint16_t const pde_idx, uint16_t const index) {
    ASSERT(pde_idx < PAGEDIR_ENTRIES_COUNT);
    ASSERT(index < PAGETABLE_ENTRIES_COUNT);

    // We first check that the PDE is actually present.
    struct pagedir_entry const * const pde = __get_pde(vm, pde_idx);
    ASSERT(pde->present);

    if (paging_enabled()) {
        // PDE is marked present, so we know that the page table exists and we
        // can move one with the computation of the virtual address
        // corresponding to the entry.
        v_addr const page_table_addr = 0xFFC00000 + (pde_idx << 12);
        size_t const offset = index * sizeof(struct pagetable_entry);
        v_addr const pte_addr = page_table_addr + offset;
        return (struct pagetable_entry *) pte_addr;
    } else {
        // With paging disabled we can read the page table directly.
        struct pagetable * const pagetable =
            (struct pagetable*)(pde->pagetable_addr << 12);
        return pagetable->entry + index;
    }
}

bool
paging_enabled(void) {
    uint32_t const cr0 = read_cr0();
    uint32_t const pg_bit = 31;
    return cr0 & (1UL << pg_bit);
}

void
paging_map(struct vm * const vm,
           struct frame_alloc * const allocator,
           p_addr const start, 
           size_t const size,
           v_addr const dest) {
    ASSERT(vm);
    // Iterate over all addresses that are multiple of PAGE_SIZE.
    uint32_t const inc = PAGE_SIZE;
    for (v_addr v = dest, p = start; v < dest + size; v += inc, p += inc) {
        uint32_t const page_idx = v / PAGE_SIZE;
        uint32_t const pde_idx = page_idx / PAGEDIR_ENTRIES_COUNT;
        uint32_t const pte_idx = page_idx % PAGETABLE_ENTRIES_COUNT;

        // Check the existance of the PDE.
        struct pagedir_entry * const pde = __get_pde(vm, pde_idx);
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

        // Get a pointer on the PTE so that we can modify it from the virtual
        // address space.
        struct pagetable_entry * const pte = __get_pte(vm, pde_idx, pte_idx);

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
    // Paging has beed enabled in early boot and the kernel is currently mapped
    // twice: identity mapping and higher half.
    // Get rid of the identity mapping.
    p_addr const p_kernel_start = (v_addr)KERNEL_START - 0xC0000000;
    uint16_t pde_idx = (p_kernel_start / PAGE_SIZE) / PAGEDIR_ENTRIES_COUNT;

    struct pagedir_entry * pde = __get_pde(&KERNEL_PAGEDIRECTORY, pde_idx);
    while(pde->present) {
        pde->present = 0;
        pde_idx ++;
        pde = __get_pde(&KERNEL_PAGEDIRECTORY, pde_idx);
    }
}

void
paging_dump_pagedir(void) {
    for (uint16_t pdi = 0; pdi < PAGEDIR_ENTRIES_COUNT; ++pdi) {
        struct pagedir_entry const * const pde = __get_pde(&KERNEL_PAGEDIRECTORY, pdi);
        if (!pde->present) {
            continue;
        }

        LOG("[%d] -> table at %p\n", pdi, pde->pagetable_addr << 12);
        for (uint16_t pti = 0; pti < PAGETABLE_ENTRIES_COUNT; ++pti) {
            struct pagetable_entry const * const pte = __get_pte(&KERNEL_PAGEDIRECTORY, pdi, pti);
            if (!pte->present) {
                continue;
            }
            LOG("  [%d] -> frame at %p\n", pti, pte->pageframe_addr << 12);
        }
    }
}
