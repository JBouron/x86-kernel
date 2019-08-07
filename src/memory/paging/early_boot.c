// This file contains the paging initialization performed in early boot. Those
// functions are called first as we boot, before the main.
// There is no associated .h file since those functions will never be used
// elsewhere.
//
// This kernel uses higher-half mapping for the kernel.

// When calling those functions the addressing is obviously physical, thus we
// need to be extra careful when accessing globals.

// Some useful macros to get the physical address of a virtual address for the
// higher-half mapping, which is a simple offset.
#define KERNEL_VIRT_START_ADDR  (0xC0000000)
#define PHYS_ADDR(addr) ((p_addr)(((uint8_t*)(addr)) - KERNEL_VIRT_START_ADDR))

#include <memory/paging/paging.h>
#include <memory/paging/alloc/early_boot.h>
#include <memory/paging/alloc/alloc.h>
#include <memory/paging/alloc/simple.h>
#include <utils/kernel_map.h>
#include <utils/memory.h>

void
__early_boot__setup_paging(void);

void
__early_boot__finish_paging(void);

// This function will enable paging with the page_dir as CR3. However it will
// not jump into the higher half kernel yet, but stay in the identity mapping
// instead.
extern void
__early_boot__enable_paging(p_addr const page_dir);

// Create a mapping between physical and virtual memory address space.
// The physical region start:start+size will be mapped to the virtual region
// dest:dest+size.
// The addresses must be PAGE_SIZE aligned and the size a multiple of PAGE_SIZE
// This code is to be ran in early boot, that is with paging *disabled*. As
// such it can easily modify the page directory and page tables using pohysical
// addresses.
// Once paging is enabled this function shall *never* be used.
static void
__early_boot__map(pagedir_t root_page_dir,
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


// Create the identity and higher-half mappings and return the physical address
// of the kernel page directory that needs to be stored in CR3.
static p_addr
__early_boot__create_mappings(p_addr * const last_allocated_frame) {
    // To setup paging we create two mappings of the kernel:
    //      _ An identity mapping, this is required since the next instruction
    //      fetched after enabling paging will still use physical addressing.
    //      _ A mapping in the higher half starting at KERNEL_VIRT_START_ADDR.

    // Compute the memory area occupied by the kernel. We want to map from
    // address KERNEL_START to KERNEL_END. 
    p_addr const start = PHYS_ADDR(KERNEL_START);
    p_addr const size = KERNEL_SIZE;

    // For the duration of the early boot we are using a *very* simple frame
    // allocator allocating frames after the kernel.
    // the RAM region 0x00100000-0x00EFFFFF is free of use (and also contains
    // the kernel) we can start directly after the kernel sections.
    // The allocator struct needs to be in the stack, since we can't dynamically
    // allocate things yet !
    struct early_boot_frame_alloc_t _eb_allocator;
    // Init the allocator.
    fa_early_boot_init(&_eb_allocator);

    // Use the frame_alloc_t abstraction from now on.
    struct frame_alloc_t *alloc = (struct frame_alloc_t*)&_eb_allocator;

    // Allocate the page directory.
    p_addr const page_dir_addr = alloc->alloc_frame(alloc);
    pagedir_t page_dir = (pagedir_t) page_dir_addr;

    // Create identity mapping.
    __early_boot__map(page_dir, alloc, start, size, start);

    // Create higher-half mapping.
    v_addr const higher_half_start = KERNEL_VIRT_START_ADDR + start;
    __early_boot__map(page_dir, alloc, start, size, higher_half_start);

    // In this kernel we use a recursive entry to modify the page tables after
    // paging is enabled. That is, in the page directories, the last entry
    // points to the directory itself: 0xFFC00000 -> <Start addr of dir>.
    // Set up this entry here.
    uint16_t last_pde_idx = PAGEDIR_ENTRIES_COUNT - 1;
    struct pagedir_entry_t * const last_pde = page_dir + last_pde_idx;
    memzero((uint8_t*) last_pde, sizeof(*last_pde));
    last_pde->pagetable_addr = page_dir_addr >> 12;
    last_pde->writable = 1;
    // We need to disable caching here so modifications go straight to memory.
    last_pde->cacheable = 0;
    last_pde->present = 1;

    // Output the address of the last allocated frame in RAM, that will be used
    // by the next frame allocator.
    *last_allocated_frame = _eb_allocator.next_frame_addr - PAGE_SIZE;

    // Return the address of the kernel page dir to be loaded in CR3.
    return page_dir_addr;
}

void
__early_boot__setup_paging(void) {
    // Create the two (identity and higher-half) mappings and retrieve the
    // address of the kernel page directory.
    p_addr last_allocated_frame = 0x0;
    // This will contain the address of the last allocated frame.
    p_addr const kernel_page_dir = __early_boot__create_mappings(
        &last_allocated_frame);

    // Enable paging.
    __early_boot__enable_paging(kernel_page_dir);
    // Paging is from now on enabled.

    // Note: While paging is enabled, we are still using the identity mapping
    // here. Yet we can now access the global variables.
    // Set the KERNEL_PAGEDIRECTORY global to the physical address of the
    // kernel's page directory.
    KERNEL_PAGEDIRECTORY = (pagedir_t)kernel_page_dir;

    // We can now setup the frame allocator.
    struct simple_frame_alloc_t * fa_simple = (struct simple_frame_alloc_t *)
        &FRAME_ALLOCATOR;
    fa_simple_alloc_init(fa_simple, last_allocated_frame + PAGE_SIZE);
}

// To be called once paging as beed enabled and *after* jumping into the higher
// half. This function will remove the identity mapping and setup the frame
// allocator.
void
__early_boot__finish_paging(void) {
    // Paging has been enabled, and we have jumped to the higher-half kernel. We
    // can now get rid of the identity mapping and set some globals related to
    // paging.

    // Go over each PDE starting from the first one. This gets rid of the
    // identity mapping.
    v_addr const start_addr = (v_addr)KERNEL_START;
    uint32_t const pde_start = (start_addr / PAGE_SIZE) / PAGEDIR_ENTRIES_COUNT;
    struct pagedir_entry_t * entry = KERNEL_PAGEDIRECTORY + pde_start;
    while(entry->present) {
        // The PDE is marked as present, we can remove it.
        entry->present = 0;
        // Go to the next entry.
        entry++;
    }
    // Important note: The page table used by the PDEs of the identity mapping
    // can be overwritten from now.
}

