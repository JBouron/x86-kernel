#include <memory/paging/paging.h>
#include <utils/math.h>
#include <utils/memory.h>
#include <utils/kernel_map.h>
#include <memory/paging/alloc/alloc.h>
#include <memory/paging/alloc/simple.h>

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

// This is the main frame allocator used by the paging subsystem.
struct simple_frame_alloc PAGING_FRAME_ALLOCATOR;

#define KERNEL_VIRT_START_ADDR  (0xC0000000)
#define PHYS_ADDR(addr) ((p_addr)(((uint8_t*)(addr)) - KERNEL_VIRT_START_ADDR))

static p_addr
__create_kernel_mappings(void) {
    // To setup paging we create two mappings of the kernel:
    //      _ An identity mapping, this is required since the next instruction
    //      fetched after enabling paging will still use physical addressing.
    //      _ A mapping in the higher half starting at KERNEL_VIRT_START_ADDR.

    // Compute the memory area occupied by the kernel. We want to map from
    // address KERNEL_START to KERNEL_END. 
    p_addr const start = PHYS_ADDR(KERNEL_START);
    p_addr const size = KERNEL_SIZE;

    // Use the frame_alloc_t abstraction from now on.
    struct frame_alloc *alloc = (struct frame_alloc*)PHYS_ADDR(&PAGING_FRAME_ALLOCATOR);

    // Allocate the page directory.
    p_addr const page_dir_addr = alloc->alloc_frame(alloc);
    struct pagedir * page_dir = (struct pagedir*) page_dir_addr;

    // Setup a temporary vm struct during paging setup.
    struct vm temp_vm = {
        .pagedir_addr = page_dir_addr,
    };

    // Create identity mapping.
    paging_map(&temp_vm, alloc, start, size, start);

    // Create higher-half mapping.
    v_addr const higher_half_start = KERNEL_VIRT_START_ADDR + start;
    paging_map(&temp_vm, alloc, start, size, higher_half_start);

    // In this kernel we use a recursive entry to modify the page tables after
    // paging is enabled. That is, in the page directories, the last entry
    // points to the directory itself: 0xFFC00000 -> <Start addr of dir>.
    // Set up this entry here.
    uint16_t last_pde_idx = PAGEDIR_ENTRIES_COUNT - 1;
    struct pagedir_entry * const last_pde = page_dir->entry + last_pde_idx;
    memzero((uint8_t*) last_pde, sizeof(*last_pde));
    last_pde->pagetable_addr = page_dir_addr >> 12;
    last_pde->writable = 1;
    // We need to disable caching here so modifications go straight to memory.
    last_pde->cacheable = 0;
    last_pde->present = 1;

    // Return the address of the kernel page dir to be loaded in CR3.
    return page_dir_addr;
}

extern void
__do_enable_paging(p_addr const pagedir_addr);

extern void
__jump_to_higher_half(v_addr const target);

void
paging_enable(v_addr const target) {
    // Init frame allocator.    
    p_addr const kernel_end = PHYS_ADDR(KERNEL_END);
    p_addr const alloc_next_free = (1 + kernel_end / PAGE_SIZE) * PAGE_SIZE;

    p_addr const alloc_addr = PHYS_ADDR((p_addr)&PAGING_FRAME_ALLOCATOR);
    struct simple_frame_alloc* const alloc = (struct simple_frame_alloc *)alloc_addr;
    fa_simple_alloc_init(alloc, alloc_next_free);

    // Setup the two mappings.
    p_addr const kernel_page_dir = __create_kernel_mappings();

    // Enable paging with the page directory allocated earlier. This will not
    // jump in the higher half yet.
    __do_enable_paging(kernel_page_dir);
    // At this point the higher half is mapped but we are still in the identity
    // map. We can therefore access the global variables.

    // Setup the kernel's vm struct.
    KERNEL_PAGEDIRECTORY.pagedir_addr = kernel_page_dir;

    // Reinit the frame allocator to use pointers valid in the higher half.
    fa_simple_alloc_init(alloc, PAGING_FRAME_ALLOCATOR.next_frame_addr);
    // Set the global allocator.
    FRAME_ALLOCATOR = (struct frame_alloc*)&PAGING_FRAME_ALLOCATOR;

    // Everything is setup, we are now ready to jump into the higher half.
    __jump_to_higher_half(target);

    // The above call will destroy the stack and never return.
    __UNREACHABLE__;
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
