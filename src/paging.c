#include <paging.h>
#include <debug.h>
#include <frame_alloc.h>
#include <memory.h>
#include <math.h>
#include <kernel_map.h>

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
struct page_dir_t {
    union pde_t entry[1024];
};

// A page table structure.
struct page_table_t {
    union pte_t entry[1024];
};

// Allocate a new page directory.
// @return: The _physical_ address of the freshly allocated page directory.
static struct page_dir_t * alloc_page_dir(void) {
    return (struct page_dir_t*) alloc_frame();
}

// Allocate a new page table.
// @return: The _physical_ address of the freshly allocated page table.
static struct page_table_t * alloc_page_table(void) {
    return (struct page_table_t*) alloc_frame();
}

// The _physical_ address of the kernel page directory.
static struct page_dir_t * KERNEL_PAGE_DIR = 0x0;

// Create the recursive entry on the last entry of a page directory.
// @param page_dir: The page directory in which the recursive entry will be
// created. Note: This needs to be a _physical_ address.
static void create_recursive_entry(struct page_dir_t * const page_dir) {
    // For now this function will not work if paging has already been enabled.
    // TODO :#:
    ASSERT(!cpu_paging_enabled());

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
static struct page_dir_t * get_curr_page_dir_vaddr(void) {
    ASSERT(cpu_paging_enabled());
    uint32_t const vaddr = (1023 << 22) | (1023 << 12);
    return (struct page_dir_t *) vaddr;
}

// Get the virtual address "pointing" to a page table under the current page
// directory loaded in CR3, using the recursive entry.
// @param table_index: The index of the page table to get a pointer to.
// @return: The virtual address of the page table.
// Note: This function will not work if paging is disabled or CR3 does not
// contain a valid page directory with a recursive entry.
static struct page_table_t * get_page_table_vaddr(uint16_t const table_index) {
    uint32_t const vaddr = (1023 << 22) | (((uint32_t)table_index) << 12);
    return (struct page_table_t *) vaddr;
}

// Remove the identity mapping created during paging initialization.
// Note: This function assumes that paging has been enabled and that nothing
// uses the identity map (EIP, stack, GDTR, IDTR, ...).
static void remove_identity_mapping(void) {
    // Since we are now using paging, we need to use the recursive entry to
    // modify the page directory.
    struct page_dir_t * const page_dir = get_curr_page_dir_vaddr();
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
static void map_page(struct page_dir_t * const page_dir,
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
    if (!page_dir->entry[pde_idx].present) {
        // The table for this index is not present, we need to allocate it and
        // set it up.
        struct page_table_t * const new_table = alloc_page_table();
        union pde_t pde;
        // Try to be as inclusive as possible for the PDE. The PTEs will
        // enforced the actual attributes.
        pde.writable = 1;
        pde.write_through = 0;
        pde.cache_disable = 0;

        // For now we are mapping the kernel which should not be accessible to
        // the user.
        pde.user_accessible = 0;

        pde.page_table_addr = ((uint32_t)new_table) >> 12;
        pde.present = 1;

        // The same page table can be used for both mappings.
        page_dir->entry[pde_idx] = pde;
    }

    struct page_table_t * const page_table = cpu_paging_enabled() ?
        get_page_table_vaddr(pde_idx) :
        (struct page_table_t*)(page_dir->entry[pde_idx].page_table_addr << 12);

    uint32_t const pte_idx = pte_index(vaddr);

    // The entry should not be present, otherwise we have a conflict.
    if (page_table->entry[pte_idx].present) {
        PANIC("Address %p is already mapped.", vaddr);
    }

    union pte_t pte;
    pte.writable = flags & VM_WRITE;
    pte.write_through = flags & VM_WRITE_THROUGH;
    pte.cache_disable = flags & VM_CACHE_DISABLE;
    pte.global = 1;
    pte.user_accessible = flags & VM_USER;
    pte.zero = 0;
    pte.present = 1;
    pte.frame_addr = ((uint32_t)paddr) >> 12;

    page_table->entry[pte_idx] = pte;
}

// As part as the paging initialization routine, create two mappings:
// _ One identity mapping of the entire kernel.
// _ One higher half mapping where the kernel is relocated at KERNEL_PHY_OFFSET
// in virtual memory.
// @param pd: The _physical_ address (paging is not yet enabled) of the page
// directory of the kernel.
static void create_identity_and_higher_half_mappings(struct page_dir_t * pd) {
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
        flags |= VM_GLOBAL;

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
    struct page_dir_t * const page_dir = alloc_page_dir();
    memzero(page_dir, PAGE_SIZE);

    // Create both mappings.
    create_identity_and_higher_half_mappings(page_dir);

    // Set the last entry in the page directory to point to itself. This is to
    // implement recursive page tables which makes it easier to modify page
    // directories and page tables once paging is enabled.
    create_recursive_entry(page_dir);

    // Set the global variable containing the physical address of the kernel's
    // page directory.
    *(struct page_dir_t**)to_phys(&KERNEL_PAGE_DIR) = page_dir;

    // Load the page dir in CR3 and enable paging.
    cpu_set_cr3(page_dir);
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
    struct gdt_desc_t gdtr;
    cpu_sgdt(&gdtr);
    gdtr.base = to_virt(gdtr.base);
    cpu_lgdt(&gdtr);

    // Fixup the return address of this function call. We access this address
    // using the value of ESP given as arg.
    uint32_t * const ret_addr = (uint32_t*)(to_virt(esp) - 8);
    *ret_addr = (uint32_t)to_virt((void const*)*ret_addr);
}

// Unmap a virtual page.
// @param vaddr: The address of the virtual page. Must be 4Kib aligned.
static void unmap_page(void const * const vaddr) {
    ASSERT(is_4kib_aligned(vaddr));
    struct page_dir_t * const page_dir = get_curr_page_dir_vaddr();

    uint32_t const pde_idx = pde_index(vaddr);
    if (!page_dir->entry[pde_idx].present) {
        PANIC("Address %p is not mapped.", vaddr);
    }

    struct page_table_t * const page_table = get_page_table_vaddr(pde_idx);
    uint32_t const pte_idx = pte_index(vaddr);

    // The entry should be present, otherwise we have a conflict.
    if (!page_table->entry[pte_idx].present) {
        PANIC("Address %p is not mapped.", vaddr);
    }

    memzero(&page_table->entry[pte_idx], sizeof(*page_table->entry));
}

// Map a virtual memory region to a physical one.
// @param paddr: The physical address to map the virtual address to. Must be
// 4KiB aligned.
// @param vaddr: The virtual address. Must be 4KiB aligned.
// @param len: The length in byte of the memory region. Note that mapping should
// ideally be multiple of PAGE_SIZE (since that is the granularity), but this
// field does not have to be.
// @param flags: The attributes of the mapping. See VM_* macros in paging.h.
void paging_map(void const * const paddr,
                void const * const vaddr,
                size_t const len,
                uint32_t const flags) {
    ASSERT(is_4kib_aligned(paddr));
    ASSERT(is_4kib_aligned(vaddr));

    uint32_t const num_frames = ceil_x_over_y_u32(len, PAGE_SIZE);
    for (size_t i = 0; i < num_frames; ++i) {
        void const * const pchunk = paddr + i * PAGE_SIZE;
        void const * const vchunk = vaddr + i * PAGE_SIZE;
        map_page(get_curr_page_dir_vaddr(), pchunk, vchunk, flags);
    }
    cpu_invalidate_tlb();
}

// Unmap a virtual memory region.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
void paging_unmap(void const * const vaddr,
                  size_t const len) {
    ASSERT(is_4kib_aligned(vaddr));

    uint32_t const num_frames = ceil_x_over_y_u32(len, PAGE_SIZE);
    for (size_t i = 0; i < num_frames; ++i) {
        unmap_page(vaddr + i * PAGE_SIZE);
    }
    cpu_invalidate_tlb();
}
