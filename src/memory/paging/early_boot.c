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
#include <includes/kernel_map.h>

void
__early_boot__setup_paging(void);

void
__early_boot__finish_paging(void);

// This function will enable paging with the page_dir as CR3. However it will
// not jump into the higher half kernel yet, but stay in the identity mapping
// instead.
extern void
__early_boot__enable_paging(p_addr const page_dir);


static p_addr
__early_boot__create_mappings(void) {
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
    p_addr const page_dir = alloc->alloc_frame(alloc);

    // Create identity mapping.
    paging_map((pagedir_t)page_dir, alloc, start, size, start);

    // Create higher-half mapping.
    v_addr const higher_half_start = KERNEL_VIRT_START_ADDR + start;
    paging_map((pagedir_t)page_dir, alloc, start, size, higher_half_start);

    // Create the back pointer, that is the mapping 0xFFFFF000 ->
    // page_dir. This back-pointer will allow us to get the physical address of
    // the kernel page directory after paging is enabled.
    paging_map((pagedir_t)page_dir, alloc, page_dir, PAGE_SIZE, 0xFFFFF000);

    // Return address of the kernel page dir to be loaded in CR3.
    return page_dir;
}

void
__early_boot__setup_paging(void) {
    // Create the two (identity and higher-half) mappings and retrieve the
    // address of the kernel page directory.
    p_addr const kernel_page_dir = __early_boot__create_mappings();

    // Enable paging.
    __early_boot__enable_paging(kernel_page_dir);
    // Paging is from now on enabled.

    // Note: While paging is enabled, we are still using the identity mapping
    // here. Yet we can now access the global variables.
    KERNEL_PAGEDIRECTORY = (pagedir_t)kernel_page_dir;
}

void
__early_boot__finish_paging(void) {
    // Paging has been enabled, and we have jumped to the higher-half kernel. We
    // can now get rid of the identity mapping and set some globals related to
    // paging.

    // Go over each PDE starting from the first one.
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

    // We can now setup the frame allocator.
}

