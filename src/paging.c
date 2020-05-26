#include <paging.h>
#include <debug.h>
#include <frame_alloc.h>
#include <memory.h>
#include <math.h>
#include <kernel_map.h>
#include <lock.h>
#include <ipm.h>
#include <smp.h>
#include <addr_space.h>

// This is the definition of an entry of a page directory.
union pde_t {
    uint32_t val;
    struct {
        // Indicate if the entry is present.
        uint8_t present : 1;
        // If set, the memory region described by this entry is writable.
        uint8_t writable : 1;
        // If set, the memory region described by this entry is user accessible.
        uint8_t user_accessible : 1;
        // If set, the memory region described by this entry is write through.
        uint8_t write_through : 1;
        // If set, the memory region described by this entry is not cached.
        uint8_t cache_disable : 1;
        // [CONST] Indicates if this entry has been accessed by the CPU.
        uint8_t accessed : 1;
        // [CONST] Ignored field. Also contains the PS bit which is only used
        // with 4MB pages. This is not supported in this kernel.
        // Note: Using a uint16_t here to avoid a warning "offset of packed
        // bit-field 'ignored' has changed in GCC 4.4".
        uint16_t ignored : 6;
        // Physical address (top 20 bits) of the page table pointed by this
        // entry.
        uint32_t page_table_addr : 20;
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(union pde_t) == 4, "");

// This is the definition of an entry of a page table.
union pte_t {
    uint32_t val;
    struct {
        // Indicate if the entry is present.
        uint8_t present : 1;
        // If set, the memory region described by this entry is writable.
        uint8_t writable : 1;
        // If set, the memory region described by this entry is user accessible.
        uint8_t user_accessible : 1;
        // If set, the memory region described by this entry is write through.
        uint8_t write_through : 1;
        // If set, the memory region described by this entry is not cached.
        uint8_t cache_disable : 1;
        // [CONST] Indicates if this entry has been accessed by the CPU.
        uint8_t accessed : 1;
        // [CONST] ndicates if the page pointed by this entry is dirty.
        uint8_t dirty : 1;
        // PAT, not supported, must be 0.
        uint8_t zero : 1;
        // If set, prevents the TLB from updating the address in its cache if
        // CR3 is reset.
        uint8_t global : 1;
        // [CONST] Ignored field.
        uint8_t ignored : 3;
        // Physical address (top 20 bits) of the page pointed by this entry.
        uint32_t frame_addr : 20;
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(union pte_t) == 4, "");

// A page directory structure.
struct page_dir {
    union pde_t entry[1024];
};

// A page table structure.
struct page_table {
    union pte_t entry[1024];
};

// Compare two PTEs.
// @param a: The first PTE.
// @param b: The second PTE.
// @return: true if a and b are identical, false otherwise.
static bool compare_ptes(union pte_t const a, union pte_t const b) {
    // Two PTEs are identical if all their fields except accessed, dirty and
    // ignored are identical.
    return a.present == b.present &&
    a.writable == b.writable &&
    a.user_accessible == b.user_accessible &&
    a.write_through == b.write_through &&
    a.cache_disable == b.cache_disable &&
    a.zero == b.zero &&
    a.global == b.global &&
    a.frame_addr == b.frame_addr;
}

// Allocate a new page directory.
// @return: The _physical_ address of the freshly allocated page directory.
static struct page_dir * alloc_page_dir(void) {
    return (struct page_dir*) alloc_frame();
}

// Allocate a new page table.
// @return: The _physical_ address of the freshly allocated page table.
static struct page_table * alloc_page_table(void) {
    return (struct page_table*) alloc_frame();
}

// Create the recursive entry on the last entry of a page directory.
// @param page_dir: The page directory in which the recursive entry will be
// created. Note: This needs to be a _physical_ address.
static void create_recursive_entry(struct page_dir * const page_dir) {
    // For now this function will not work if paging has already been enabled.
    // TODO :#:
    union pde_t recursive_entry;
    // This is a must as the goal of the recursive entry is to write to the
    // dir/table.
    recursive_entry.writable = 1;
    // Make sure the updates on the dir/table goes straight to memory when
    // dealing with the recursive entry.
    recursive_entry.write_through = 1;
    recursive_entry.cache_disable = 1;
    recursive_entry.user_accessible = 0;
    // This is why this function is not working with paging enabled. TODO :#:
    recursive_entry.page_table_addr = ((uint32_t)page_dir) >> 12;
    recursive_entry.present = 1;

    page_dir->entry[1023] = recursive_entry;
}

// Get the virtual address "pointing" to the current page directory loaded in
// CR3, using the recursive entry.
// @return: The virtual address of the page directory.
// Note: This function will not work if paging is disabled or CR3 does not
// contain a valid page directory with a recursive entry.
static struct page_dir * get_curr_page_dir_vaddr(void) {
    ASSERT(cpu_paging_enabled());
    uint32_t const vaddr = (1023 << 22) | (1023 << 12);
    return (struct page_dir *) vaddr;
}

// Get the virtual address "pointing" to a page table under the current page
// directory loaded in CR3, using the recursive entry.
// @param table_index: The index of the page table to get a pointer to.
// @return: The virtual address of the page table.
// Note: This function will not work if paging is disabled or CR3 does not
// contain a valid page directory with a recursive entry.
static struct page_table * get_page_table_vaddr(uint16_t const table_index) {
    uint32_t const vaddr = (1023 << 22) | (((uint32_t)table_index) << 12);
    return (struct page_table *) vaddr;
}

// Remove the identity mapping created during paging initialization.
// Note: This function assumes that paging has been enabled and that nothing
// uses the identity map (EIP, stack, GDTR, IDTR, ...).
static void remove_identity_mapping(void) {
    // Since we are now using paging, we need to use the recursive entry to
    // modify the page directory.
    struct page_dir * const page_dir = get_curr_page_dir_vaddr();
    uint32_t const num_frames = ceil_x_over_y_u32(
        (uint32_t)to_phys(KERNEL_END_ADDR), PAGE_SIZE);
    // Compute the number of page tables that will need to be allocated.
    uint32_t const num_page_tables = ceil_x_over_y_u32(num_frames, 1024);
    for (uint16_t i = 0; i < num_page_tables; ++i) {
        // The mapping _must_ be present. Or something weird is going on.
        ASSERT(page_dir->entry[i].present);
        // Zero the entire entry, cleaner.
        memzero(page_dir->entry + i, sizeof(*page_dir->entry));
    }
}

// Compute the Page Directory Entry index corresponding to a virtual address.
// @param vaddr: The virtual address to extract the PDE index from.
// @return: The PDE index of the address, that is the 10 most significant bits.
static inline uint16_t pde_index(void const * const vaddr) {
    return ((uint32_t)vaddr) >> 22;
}

// Compute the Page Table Entry index corresponding to a virtual address.
// @param vaddr: The virtual address to extract the PTE index from.
// @return: The PTE index of the address, that is bits 12:21 of the address.
static inline uint16_t pte_index(void const * const vaddr) {
    return (((uint32_t)vaddr) >> 12) & 0x3FF;
}

// Map a physical frame to a virtual page.
// @param page_dir: The page directory in which to create the mapping. Note that
// this could be either a physical address, if paging is not yet enabled, or a
// virtual address computed with the recursive entry.
// @param paddr: The physical address to be mapped to virtual memory. This
// address must be 4KiB aligned.
// @param vaddr: The virtual address that should map to `paddr` in the page
// structure. This address must be 4KiB aligned.
// @param flags: Flags indicating the attributes of the mapping. See macros VM_*
// in paging.h.
// Note: Even though this function works whether or not paging is enabled, once
// paging is enabled, the page_dir parameter must be the virtual address of the
// page directory currently loaded in CR3, otherwise the behavior is undefined.
static void map_page(struct page_dir * const page_dir,
                     void const * const paddr,
                     void const * const vaddr,
                     uint32_t const flags) {
    ASSERT(is_4kib_aligned(paddr));
    ASSERT(is_4kib_aligned(vaddr));

    if (cpu_paging_enabled() && !is_higher_half(page_dir)) {
        // If paging is enabled then page_dir must be a virtual address.
        PANIC("Must use a virtual pointer to modify page directory.");
    }

    uint32_t const pde_idx = pde_index(vaddr);
    bool page_table_allocated = false;
    if (!page_dir->entry[pde_idx].present) {
        // The table for this index is not present, we need to allocate it and
        // set it up.
        struct page_table * const new_table = alloc_page_table();
        page_table_allocated = true;
        union pde_t pde;
        // Try to be as inclusive as possible for the PDE. The PTEs will
        // enforced the actual attributes.
        pde.writable = 1;
        pde.write_through = 0;
        pde.cache_disable = 0;
        // Reset accessed bit.
        pde.accessed = 0;

        // For now we are mapping the kernel which should not be accessible to
        // the user.
        pde.user_accessible = 0;

        pde.page_table_addr = ((uint32_t)new_table) >> 12;
        pde.present = 1;

        // The same page table can be used for both mappings.
        page_dir->entry[pde_idx] = pde;
    }

    struct page_table * const page_table = cpu_paging_enabled() ?
        get_page_table_vaddr(pde_idx) :
        (struct page_table*)(page_dir->entry[pde_idx].page_table_addr << 12);

    if (page_table_allocated) {
        // If the page table has been freshly allocated zero it, this is to
        // avoid garbage.
        memzero(page_table, sizeof(*page_table));
    }

    union pte_t pte;
    // Need to be extremely careful with the bitwise op here. The value stored
    // in the attributes of the PTE need to be 0 or 1, anything higher might not
    // map to what it seems (eg. pte.write_through = 2 will write 0 instead).
    // This is because of the bitfields with size 1 bit.
    pte.writable = (bool)(flags & VM_WRITE);
    pte.write_through = (bool)(flags & VM_WRITE_THROUGH);
    pte.cache_disable = (bool)(flags & VM_CACHE_DISABLE);
    pte.global = (bool)!(flags & VM_NON_GLOBAL);
    pte.user_accessible = (bool)(flags & VM_USER);
    // Reset accessed and dirty bits.
    pte.accessed = 0;
    pte.dirty = 0;
    pte.zero = 0;
    pte.present = 1;
    pte.frame_addr = ((uint32_t)paddr) >> 12;

    uint32_t const pte_idx = pte_index(vaddr);

    if (page_table->entry[pte_idx].present) {
        // If there was already an entry at this index compare with the new
        // entry that we want to insert. If both entries are identical then do
        // nothing, otherwise panic.
        if (!compare_ptes(pte, page_table->entry[pte_idx])) {
            PANIC("Overwriting previous PTE when mapping address %p", vaddr);
        }
    }

    page_table->entry[pte_idx] = pte;
}

// As part as the paging initialization routine, create two mappings:
// _ One identity mapping of the entire kernel.
// _ One higher half mapping where the kernel is relocated at KERNEL_PHY_OFFSET
// in virtual memory.
// @param pd: The _physical_ address (paging is not yet enabled) of the page
// directory of the kernel.
static void create_identity_and_higher_half_mappings(struct page_dir * pd) {
    // Map the virtual addresses KERNEL_PHY_OFFSET -> KERNEL_END_ADDR to 0x0 ->
    // <whatever the size of the kernel is>.
    void const * const start = KERNEL_PHY_OFFSET;
    void const * const end = (void*) KERNEL_END_ADDR;

    for(void const * ptr = start; ptr < end; ptr += PAGE_SIZE) {
        uint32_t flags = 0;
        if (in_low_mem(ptr)) {
            // Addresses mapped to low memory should be write through with
            // cache disabled as we expect memory devices mapped there (incl.
            // VGA buffer).
            flags = VM_WRITE | VM_WRITE_THROUGH | VM_CACHE_DISABLE;
        } else if (in_text_section(ptr) || in_rodata_section(ptr)) {
            // Addresses mapped to the .text and .rodata section should be
            // marked as readonly.
            flags = 0;
        } else {
            // Any other addresses are read/write.
            flags = VM_WRITE;
        }

        void const * const paddr = to_phys(ptr);
        void const * const vaddr = ptr;
        // Map the higher-half address to the physical address.
        map_page(pd, paddr, vaddr, flags);
        // Identity map the low addresses.
        map_page(pd, paddr, paddr, flags);
    }
}

// Initialize and enable paging. After calling this function the processor uses
// the higher-half kernel (EIP, stack and GDTR point to the higher-half).
// @param esp: The stack pointer value right before calling this function. This
// parameter is important as it allows us to fixup the return address pushed on
// the stack as a _physical_ address.
void init_paging(void const * const esp) {
    // Initializing paging involves creating two mapping before enabling the PG
    // bit in CR0:
    //     * An identity mapping, which maps the low virtual memory to low
    //     physical memory. This is used so that we don't page fault (and triple
    //     fault) after enabling the PG bit. This leaves us time to setup the
    //     stack, etc...
    //     * An higher-half mapping, which maps the virtual addresses
    //     KERNEL_PHY_OFFSET (0xC0000000) to KERNEL_END_ADDR to 0x0 ->
    //     KERNEL_SIZE. The mapping covers the 1Mib before the kernel as well as
    //     all the sections of the kernel. This is the mapping that will be used
    //     once paging is enabled.
    // Once those two mappings are created it is time to fixup the stack pointer
    // and whatever global variable that contains physical pointers (except for
    // page directory pointers).
    // After that, the identity mapping can be removed and the kernel will
    // used the higher-half mapping only.

    // Allocate a page directory for the kernel itself.
    struct page_dir * const page_dir = alloc_page_dir();
    memzero(page_dir, PAGE_SIZE);

    // Create both mappings.
    create_identity_and_higher_half_mappings(page_dir);

    // Set the last entry in the page directory to point to itself. This is to
    // implement recursive page tables which makes it easier to modify page
    // directories and page tables once paging is enabled.
    create_recursive_entry(page_dir);

    // Initialize the kernel's struct addr_space with the frame we just
    // allocated for the page directory.
    init_kernel_addr_space(page_dir);

    // Switch to the kernel's address space.
    switch_to_addr_space(get_kernel_addr_space());

    // Enable the paging bit.
    cpu_enable_paging();

    // Paging has been enabled, EIP is using the higher half mapping but the
    // stack pointer is still using the identity one, and so is the frame
    // allocator.
    // Hence, a few things need to be done before disabling the identity
    // mapping:
    //   _ Set the stack pointer to a virtual stack pointer.
    //   _ Tell the frame allocator to switch to virtual addresses for its
    //   internal state.
    extern void fixup_esp_and_ebp_to_virt(void);
    fixup_esp_and_ebp_to_virt();
    fixup_frame_alloc_to_virt();

    // We can now get rid of the identity mapping.
    remove_identity_mapping();
    cpu_invalidate_tlb();

    // Since the GDT has been setup _before_ paging, the base address in GDTR
    // uses a linear address which is equivalent to physical address. However
    // this won't work once we get rid of the identity mapping. Therefore we
    // need to fixup the base address in the GDTR.
    struct gdt_desc gdtr;
    cpu_sgdt(&gdtr);
    gdtr.base = to_virt(gdtr.base);
    cpu_lgdt(&gdtr);

    // Fixup the return address of this function call. We access this address
    // using the value of ESP given as arg.
    uint32_t * const ret_addr = (uint32_t*)(to_virt(esp) - 8);
    *ret_addr = (uint32_t)to_virt((void const*)*ret_addr);
}

// Check if a page table is empty, that is all of its entry have the present bit
// set to 0.
// @param table: The page table to test.
// @return: true if the page table is empty, false otherwise.
static bool page_table_is_empty(struct page_table const * const table) {
    for (uint16_t i = 0; i < 1024; ++i) {
        if (table->entry[i].present) {
            return false;
        }
    }
    return true;
}

// Unmap a virtual page.
// @param vaddr: The address of the virtual page. Must be 4Kib aligned.
static void unmap_page(void const * const vaddr, bool const free_phy_frame) {
    ASSERT(is_4kib_aligned(vaddr));
    struct page_dir * const page_dir = get_curr_page_dir_vaddr();

    uint32_t const pde_idx = pde_index(vaddr);
    if (!page_dir->entry[pde_idx].present) {
        PANIC("Address %p is not mapped.", vaddr);
    }

    struct page_table * const page_table = get_page_table_vaddr(pde_idx);
    uint32_t const pte_idx = pte_index(vaddr);

    // The entry should be present, otherwise we have a conflict.
    if (!page_table->entry[pte_idx].present) {
        PANIC("Address %p is not mapped.", vaddr);
    }

    // If the request to unmap also requests free'ing the mapped physical frame,
    // do it now.
    if (free_phy_frame) {
        uint32_t const foffset = page_table->entry[pte_idx].frame_addr;
        void * const faddr = (void*)((uint32_t)foffset << 12);
        free_frame(faddr);
    }

    memzero(&page_table->entry[pte_idx], sizeof(*page_table->entry));

    // Check if the current page table became empty when removing the entry
    // above. If this is the case then the frame used by this table should be
    // freed.
    if (page_table_is_empty(page_table)) {
        // This was the last page in the page table. Free the frame used by the
        // page table and mark the corresponding PDE as not present.
        page_dir->entry[pde_idx].present = 0;
        void const * const frame_addr =
            (void*)(page_dir->entry[pde_idx].page_table_addr << 12);
        free_frame(frame_addr);
    }
}

// Compute the offset within a page of an address.
// @param addr: The address to compute the page offset for.
// @return: The page offset of `addr`.
static uint32_t page_offset(void const * const addr) {
    return (uint32_t)addr & 0xFFF;
}

// Execute a TLB-Shootdown if required, that is if other cpus on the system are
// online.
static void maybe_to_tlb_shootdown(void) {
    if (aps_are_online()) {
        exec_tlb_shootdown();
    }
}

// Map a virtual memory region to a physical one.
// @param paddr: The physical address to map the virtual address to.
// @param vaddr: The virtual address.
// @param len: The length in byte of the memory region. Note that mapping should
// ideally be multiple of PAGE_SIZE (since that is the granularity), but this
// field does not have to be.
// @param flags: The attributes of the mapping. See VM_* macros in paging.h.
// Note: The addresses do not have to be 4KiB aligned, the mapping function will
// take care of that. However they need to have the same page offset (lower 12
// bits).
// Note: This function assumes that the virtual address space is locked.
static void do_paging_map(void const * const paddr,
                          void const * const vaddr,
                          size_t const len,
                          uint32_t const flags) {
    // This function accepts addresses that are not page aligned. However they
    // at least need to share the same page offset, otherwise there is probably
    // an issue in the caller.
    ASSERT(page_offset(paddr) == page_offset(vaddr));

    // Align the start addresses to page aligned addresses. Account for the
    // potentially increased size.
    void const * const start_phy = get_page_addr(paddr);
    void const * const start_virt = get_page_addr(vaddr);
    size_t const fixed_len = len + (uint32_t)(paddr - start_phy);

    // Map the pages.
    uint32_t const num_frames = ceil_x_over_y_u32(fixed_len, PAGE_SIZE);
    for (size_t i = 0; i < num_frames; ++i) {
        void const * const pchunk = start_phy + i * PAGE_SIZE;
        void const * const vchunk = start_virt + i * PAGE_SIZE;
        map_page(get_curr_page_dir_vaddr(), pchunk, vchunk, flags);
    }
}

void paging_map(void const * const paddr,
                void const * const vaddr,
                size_t const len,
                uint32_t const flags) {
    lock_curr_addr_space();
    do_paging_map(paddr, vaddr, len, flags);
    unlock_curr_addr_space();

    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();
}

// Unmap a virtual memory region.
// @param vaddr: The virtual address to unmap.
// @param len: The length of the memory area.
// @param free_phy_frame: If true, the physical frame mapped to the memory
// region will be freed.
static void do_paging_unmap(void const * const vaddr,
                            size_t const len,
                            bool const free_phy_frame) {
    void const * const start_virt = get_page_addr(vaddr);
    size_t const fixed_len = len + (uint32_t)(vaddr - start_virt);

    uint32_t const num_frames = ceil_x_over_y_u32(fixed_len, PAGE_SIZE);
    for (size_t i = 0; i < num_frames; ++i) {
        unmap_page(start_virt + i * PAGE_SIZE, free_phy_frame);
    }
}

void paging_unmap(void const * const vaddr, size_t const len) {
    lock_curr_addr_space();
    do_paging_unmap(vaddr, len, false);
    unlock_curr_addr_space();

    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();
}

void paging_unmap_and_free_frames(void const * const vaddr, size_t const len) {
    lock_curr_addr_space();
    do_paging_unmap(vaddr, len, true);
    unlock_curr_addr_space();

    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();
}

// Check if a virtual page is currently mapped to a frame in physical memory in
// the current address space.
// @param vaddr: The address (virtual) of the page. This address should be 4KiB
// aligned.
// @return: true if vaddr is mapped, false otherwise.
static bool page_is_mapped(void const * const vaddr) {
    ASSERT(is_4kib_aligned(vaddr));
    struct page_dir const * const page_dir = get_curr_page_dir_vaddr();
    uint32_t const pde_idx = pde_index(vaddr);
    uint32_t const pte_idx = pte_index(vaddr);
    if (!page_dir->entry[pde_idx].present) {
        return false;
    }
    struct page_table const * const table = get_page_table_vaddr(pde_idx);
    return table->entry[pte_idx].present;
}

// Find the next non-mapped page in the current virtual address space.
// @param start: The page address to start searching from.
// @return: The address of the next non mapped page. Note that if no page is
// available the function will panic. Therefore the return value of this
// function is always correct.
static void * find_next_non_mapped_page(void * const start) {
    void * ptr = start;
    void * const max_ptr = get_page_table_vaddr(0);
    while (ptr < max_ptr) {
        if (!page_is_mapped(ptr)) {
            return ptr;
        }
        ptr += PAGE_SIZE;
    }
    PANIC("Virtual address space full");
    return NULL;
}

// Compute the size of the hole (aka non mapped memory) starting at a virtual
// page address.
// @param vaddr: The start address to compute the size of the hole from.
// @return: The size of the hole in number of pages.
static uint32_t compute_hole_size(void const * const vaddr) {
    struct page_dir const * const page_dir = get_curr_page_dir_vaddr();
    uint32_t const start_pde_idx = pde_index(vaddr);
    uint32_t count = 0;

    for (uint16_t i = start_pde_idx; i < 1023; ++i) {
        union pde_t const * const pde = &page_dir->entry[i];
        if (!pde->present) {
            if (i == start_pde_idx) {
                count += 1024 - pte_index(vaddr);
            } else {
                count += 1024;
            }
            continue;
        }

        struct page_table const * const page_table = get_page_table_vaddr(i);
        uint16_t const start_pte_idx = i==start_pde_idx ? pte_index(vaddr) : 0;
        for (uint16_t j = start_pte_idx; j < 1024; ++j) {
            if (!page_table->entry[j].present) {
                count ++;
            } else {
                // Found the first used page. return.
                return count;
            }
        }
    }
    return count;
}

// Find a memory region in the current virtual address space with at least a
// certain number of contiguous pages non mapped.
// This function assumes the virtual address space is locked.
// @param start_addr: The start address to search from.
// @parma npages: The minimum number of contiguous non mapped pages to look for.
// @return: The start virtual address of the memory region.
static void *do_paging_find_contiguous_non_mapped_pages(void * const start_addr,
                                                        size_t const npages) {
    void * ptr = find_next_non_mapped_page(start_addr);
    bool done = false;
    while (!done) {
        uint32_t const hole = compute_hole_size(ptr);
        if (hole >= npages) {
            return ptr;
        }
        ptr = find_next_non_mapped_page(ptr + PAGE_SIZE);
    }
    PANIC("No hole big enough");
    return NULL;
}

void *paging_find_contiguous_non_mapped_pages(void * const start_addr,
                                              size_t const npages) {
    lock_curr_addr_space();
    void * const res = do_paging_find_contiguous_non_mapped_pages(start_addr,
        npages);
    unlock_curr_addr_space();
    return res;
}

void *paging_map_frames_above(void * const start_addr,
                              void ** frames,
                              size_t const npages,
                              uint32_t const flags) {
    lock_curr_addr_space();
    void * const start = do_paging_find_contiguous_non_mapped_pages(start_addr,
        npages);
    for (size_t i = 0; i < npages; ++i) {
        void const * const frame = frames[i];
        do_paging_map(frame, start + i * PAGE_SIZE, PAGE_SIZE, flags);
    }
    unlock_curr_addr_space();

    // TLB invalidation must be done outside the critical section.
    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();

    return start;
}

void paging_setup_new_page_dir(void * const page_dir_phy_addr) {
    lock_curr_addr_space();

    void * const pd_addr = this_cpu_var(curr_addr_space)->page_dir_phy_addr;

    // Because we are using do_paging_map, we need to invalidate the TLB after
    // each call. Failure to do so can lead to nasty bugs in which the cpu uses
    // a stale TLB entries and corrupts memory.
    // The TLB shootdown needs to be done outside the critical section however.
    do_paging_map(pd_addr, pd_addr, PAGE_SIZE, 0);
    cpu_invalidate_tlb();

    do_paging_map(page_dir_phy_addr, page_dir_phy_addr, PAGE_SIZE, VM_WRITE);
    cpu_invalidate_tlb();

    memzero(page_dir_phy_addr, PAGE_SIZE);

    // Copy each entry in the new page directory.
    struct page_dir const * const curr_pd = pd_addr;
    struct page_dir * const dest_pd = page_dir_phy_addr;
    uint16_t const start_idx = pde_index(KERNEL_PHY_OFFSET);
    for (uint16_t i = start_idx; i < 1023; ++i) {
        dest_pd->entry[i] = curr_pd->entry[i];
    }

    // The last PDE is the recursive entry. This one cannot be simply copied as
    // it needs to point to the frame of the page directory. To make it simple,
    // take the recursive entry of the kernel page dir and simply change the
    // page_table_addr field instead of setting all the fields manually.
    union pde_t rec = curr_pd->entry[1023];
    ASSERT(rec.page_table_addr == (uint32_t)pd_addr >> 12);
    rec.page_table_addr = ((uint32_t)page_dir_phy_addr) >> 12;
    dest_pd->entry[1023] = rec;

    do_paging_unmap(page_dir_phy_addr, PAGE_SIZE, false);
    cpu_invalidate_tlb();

    do_paging_unmap(pd_addr, PAGE_SIZE, false);
    cpu_invalidate_tlb();

    unlock_curr_addr_space();

    maybe_to_tlb_shootdown();
}

void paging_delete_page_dir(void * const page_dir_phy_addr) {
    lock_curr_addr_space();

    do_paging_map(page_dir_phy_addr, page_dir_phy_addr, PAGE_SIZE, VM_WRITE);
    cpu_invalidate_tlb();

    struct page_dir * const page_dir = page_dir_phy_addr;

    uint16_t const max_index = pde_index(KERNEL_PHY_OFFSET);
    for (uint16_t i = 0; i < max_index; ++i) {
        union pde_t pde = page_dir->entry[i];
        if (pde.present) {
            void * const page_table = (void*)(pde.page_table_addr << 12);
            free_frame(page_table);
        }
    }

    do_paging_map(page_dir_phy_addr, page_dir_phy_addr, PAGE_SIZE, VM_WRITE);
    cpu_invalidate_tlb();

    free_frame(page_dir_phy_addr);
    unlock_curr_addr_space();

    maybe_to_tlb_shootdown();
}

#include <paging.test>
