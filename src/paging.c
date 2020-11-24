#include <paging.h>
#include <debug.h>
#include <frame_alloc.h>
#include <memory.h>
#include <math.h>
#include <kernel_map.h>
#include <spinlock.h>
#include <ipm.h>
#include <smp.h>
#include <addr_space.h>
#include <error.h>
#include <segmentation.h>
#include <interrupt.h>

// Some helper constants to interact with page tables/dirs.
#define PDES_PER_PAGE       1024
#define PTES_PER_PAGE       1024
#define RECURSIVE_PDE_IDX   1023
#define TEMP_MAP_PDE_IDX    1022

// The index of the first PDE used by the kernel.
#define KERNEL_MIN_PDE_IDX  ((uint32_t)KERNEL_PHY_OFFSET >> 22)
// The index of the last PDE used by the kernel (not accounting for special PDEs
// such as temp mapping and recursive mapping). This value can be modified to
// limit the memory overhead of storing page tables for the kernel.
#define KERNEL_MAX_PDE_IDX  (RECURSIVE_PDE_IDX - 1)

// Virtual address space organization
// ==================================
//   A virtual address space is divided in user and kernel addresses. The
// separation is @ 0xC000000 (KERNEL_PHY_OFFSET). Addresses below that offset
// are considered user addresses and addresses above are kernel addresses.
//
// Recursive Entry in page dirs
// ============================
//   To make page dir/table manipulation easier, the last entry in any page
// directory (entry with index = 1023) points to the physical frame containing
// the page directory. This recurisve mapping allows us to directly modify
// mapping in the current address space loaded in CR3 by using addresses
// 0xFFC00000 - 0xFFFFFFFF.
//
// Temporary Mapping page table
// ============================
//   Entry 1023 in the page directory points to a page table used for temporary
// mappings exclusively (eg. when mapping a page dir of another address space to
// modify it). This page table (as any kernel page table) is shared between all
// cpus on the system, however each cpu has a private entry in the page table.
// In this page table, entry i is reserved for cpu i, only cpu i might modify or
// access this entry. (For now only 1 entry per cpu, in the future this could be
// extended to 4 entries per cpu).
// Because each entry is cpu private, there is no need to hold the lock on the
// address space when modifying an entry and a TLB shootdown is not needed. This
// is particularly useful when a cpu wants to modify a foreign address space and
// needs to map the other address space's page dir or table to its current
// address space in order to perform the modifications.
// Temporary mappings are ONLY used in this file while modifying paging
// structures.
//
// Page Directory summary
// ======================
//
//  +-------------+
//  |      0      | <- First PDE for user addresses.
//  +-------------+
//  |      1      | <- Second PDE for user addresses.
//  +-------------+
//        ...
//  +-------------+
//  |     767     | <- Last PDE for user addresses.
//  +-------------+
//  |     768     | <- First PDE for kernel addresses. Mapped to phy 0x0.
//  +-------------+
//  |     769     | <- Second PDE kernel addrs, mapped to phy 0x1000.
//  +-------------+
//        ...
//  +-------------+
//  |     XYZ     | <- Last PDE kernel addrs, mapped to last kernel phy frame.
//  +-------------+
//        ...
//  +-------------+
//  |    1022     | <- Points to temp mapping page table.
//  +-------------+
//  |    1023     | <- Recursive PDE entry pointing to this page directory.
//  +-------------+

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
    union pde_t entry[PDES_PER_PAGE];
};

// A page table structure.
struct page_table {
    union pte_t entry[PTES_PER_PAGE];
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
    recursive_entry.page_table_addr = ((uint32_t)to_phys(page_dir)) >> 12;
    recursive_entry.present = 1;

    page_dir->entry[RECURSIVE_PDE_IDX] = recursive_entry;
}

// Create the page table that will be used for temporary mappings when
// reading/writing from/to physical addresses.
// @param page_dir: The page directory in which the entry will be created.
static void create_temp_mapping_entry(struct page_dir * const page_dir) {
    struct page_table * const page_table = alloc_page_table();
    if (page_table == NO_FRAME) {
        PANIC("Cannot allocated temp mapping page table\n");
    }

    // Zero out the page. This is important especially in baremetal since the
    // page_table might contain garbage leading to PANIC when mapping into it.
    memzero(to_virt(page_table), PAGE_SIZE);

    LOG("Temporary mapping page table at physical address %p\n", page_table);

    ASSERT(!page_dir->entry[TEMP_MAP_PDE_IDX].present);

    union pde_t temp_map_entry;
    temp_map_entry.writable = 1;
    temp_map_entry.write_through = 1;
    temp_map_entry.cache_disable = 1;
    temp_map_entry.user_accessible = 0;
    temp_map_entry.page_table_addr = ((uint32_t)page_table) >> 12;
    temp_map_entry.present = 1;

    page_dir->entry[TEMP_MAP_PDE_IDX] = temp_map_entry;
}

// Pre-allocate page tables for all PDEs used by kernel addresses.
// @param page_dir: Pointer on the page directory to fill.
static void preallocate_kernel_page_table(struct page_dir * const page_dir) {
    for (uint32_t i = KERNEL_MIN_PDE_IDX; i < KERNEL_MAX_PDE_IDX; ++i) {
        if (page_dir->entry[i].present) {
            // The page table is already allocated for this PDE, skip.
        } else {
            void const * const page_table = alloc_frame();
            if (page_table == NO_FRAME) {
                PANIC("Cannot pre-allocate kernel page tables");
            }

            // Zero out the page. This is important especially in baremetal
            // since the page_table might contain garbage leading to PANIC when
            // mapping into it.
            memzero(to_virt(page_table), PAGE_SIZE);

            union pde_t pde;
            // Try to be as inclusive as possible for the PDE. The PTEs will
            // enforced the actual attributes.
            pde.writable = 1;
            pde.write_through = 0;
            pde.cache_disable = 0;
            // Reset accessed bit.
            pde.accessed = 0;
            pde.user_accessible = false;
            pde.page_table_addr = ((uint32_t)page_table) >> 12;
            pde.present = 1;

            // The same page table can be used for both mappings.
            page_dir->entry[i] = pde;
        }
    }
}

static bool map_page_in(struct addr_space * const addr_space,
                        void const * const paddr,
                        void const * const vaddr,
                        uint32_t const flags);

// Create a temporary mapping using the temp page table.
// @param phy_addr: The physical address to map.
// @return: The virtual address mapping to `phy_addr`. This function is
// guaranteed to succeed.
// No lock on the address space is required.
static void *create_temp_mapping(void const * const phy_addr) {
    uint32_t const cpu = cpu_apic_id();

    // Manually disable the old mapping (if existing).
    union pte_t * const entry = (union pte_t*)((RECURSIVE_PDE_IDX << 22) |
        (TEMP_MAP_PDE_IDX << 12)) + cpu;
    ASSERT(!(((uint32_t)entry) & 0x3));
    entry->present = 0;

    void * const vaddr = (void*)((TEMP_MAP_PDE_IDX << 22) | (cpu << 12));
    uint32_t const flags = VM_WRITE | VM_WRITE_THROUGH | VM_NON_GLOBAL |
        VM_CACHE_DISABLE;

    // We do not need the lock on the current address space here, because:
    //  - The page table used for the mapping already exists, hence the page dir
    //  will not be modified.
    //  - The entry in the page table is private to this cpu. No race condition
    //  possible here.
    bool const res = map_page_in(get_curr_addr_space(), phy_addr, vaddr, flags);
    // The map_page_in() call above cannot fail, this is because we will map to
    // the temp map page table and hence no page table allocation is necessary
    // and no error can occur in OOM scenarios.
    ASSERT(res);

    // No need for TLB Shootdown either. This mapping should NEVER be used by
    // other cpus.
    cpu_invalidate_tlb();
    return vaddr;
}

// Get a pointer on the page directory of an address space.
// @param addr_space: The address space to get the page directory pointer from.
// @return: The address of the page directory of `addr_space`. Note that this
// pointer will _always_ be valid no matter if paging has been enabled or not.
// This means this function can be used in early boot. This function is
// guaranteed to succeed.
// NOTE: One needs to be EXTREMELY careful when using get_page_dir() and
// get_page_table() in the same function/scope. In the case those functions are
// used to access a page dir and a page table of an address space that is NOT
// the current address space, a temporary mapping will be used. Both functions
// will map the page dir/table to the same virtual address in the temporary page
// table, effectively overwritting each other.
static struct page_dir *get_page_dir(struct addr_space * const addr_space) {
    if (!cpu_paging_enabled()) {
        // Paging is not yet enabled, but we are in higher half using the Boot
        // GDT, we need to use the to_virt() function in order to be able to
        // manipulate the physical frame.
        return to_virt(addr_space->page_dir_phy_addr);
    } else if (get_curr_addr_space() == addr_space) {
        // Paging is enabled and the target address space is the address space
        // currently loaded on this cpu. Use the recursive entry.
        uint32_t const vaddr = (RECURSIVE_PDE_IDX << 22) |
            (RECURSIVE_PDE_IDX << 12);
        return (struct page_dir *) vaddr;
    } else {
        // Paging is enabled, and we are targeting an address space that is
        // different from this cpu's address space. In this case map the page
        // directory of the target address space to the current address space
        // and return the virtual address where it has been mapped to.
        return create_temp_mapping(addr_space->page_dir_phy_addr);
    }
}

// Get a pointer on a page table of an address space.
// @param page_dir: The address of the page directory to read from. Note that
// this address must resolve to the page directory if paging has been enabled.
// @param index: The index of the page table to retreive.
// @return: The address of the page table of `addr_space` at index `index`. Note
// that this pointer will _always_ be valid no matter if paging has been enabled
// or not.  This means this function can be used in early boot. This function is
// guaranteed to succeed.
// NOTE: One needs to be EXTREMELY careful when using get_page_dir() and
// get_page_table() in the same function/scope. In the case those functions are
// used to access a page dir and a page table of an address space that is NOT
// the current address space, a temporary mapping will be used. Both functions
// will map the page dir/table to the same virtual address in the temporary page
// table, effectively overwritting each other.
static struct page_table *get_page_table(struct page_dir * const page_dir,
                                         uint16_t const index) {
    if (!cpu_paging_enabled()) {
        // Paging is not yet enabled, we can use the to_virt() on the addres of
        // the page table to manipulate it directly.
        return to_virt((void*)(page_dir->entry[index].page_table_addr << 12));
    } else if ((void*)page_dir == (void*)0xFFFFF000) {
        // The page directory address is the recursive address. That means the
        // address space used in the caller is the current address space of the
        // cpu, we can use the recursive entry for the page table as well.
        uint32_t const vaddr = (RECURSIVE_PDE_IDX << 22) |
            (((uint32_t)index) << 12);
        return (struct page_table *) vaddr;
    } else {
        // This is a different page directory than the one loaded in cr3. We
        // cannot use the recursive entry to get the virtual address of the page
        // table, map it in the current address space instead.
        void *frame = (void*)(page_dir->entry[index].page_table_addr << 12);
        return create_temp_mapping(frame);
    }
}

// Remove the identity mapping created during paging initialization.
// Note: This function assumes that paging has been enabled and that nothing
// uses the identity map (EIP, stack, GDTR, IDTR, ...).
static void remove_identity_mapping(void) {
    // Since we are now using paging, we need to use the recursive entry to
    // modify the page directory.
    struct page_dir * const page_dir = get_page_dir(get_curr_addr_space());
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

// Check if a virtual address is pointing to the temporary mapping page table.
// @param vaddr: The virtual address to test.
// @return: true if vaddr is a temp mapping address, false otherwise.
static inline bool is_temp_mapping(void const * const vaddr) {
    return pde_index(vaddr) == TEMP_MAP_PDE_IDX;
}

// Map a physical frame to a virtual page.
// @param addr_space: The address space in which the mapping should be
// performed.
// @param paddr: The physical address to be mapped to virtual memory. This
// address must be 4KiB aligned.
// @param vaddr: The virtual address that should map to `paddr` in the page
// structure. This address must be 4KiB aligned.
// @param flags: Flags indicating the attributes of the mapping. See macros VM_*
// in paging.h.
// @return: true if the mapping was successful, false otherwise. If this
// function returns false, the paging structure is guaranteed to be unchanged.
// Note: Even though this function works whether or not paging is enabled, once
// paging is enabled, the page_dir parameter must be the virtual address of the
// page directory currently loaded in CR3, otherwise the behavior is undefined.
static bool map_page_in(struct addr_space * const addr_space,
                        void const * const paddr,
                        void const * const vaddr,
                        uint32_t const flags) {
    ASSERT(is_4kib_aligned(paddr));
    ASSERT(is_4kib_aligned(vaddr));

    // Since the address space of processes are sharing the kernel address, if
    // one modify kernel mappings with any address space then the lock will not
    // protect against race condition. Hence we enforce the rule: Modifying a
    // kernel mapping can only be done in the kernel address space, not from a
    // process' address space.
    // The only exception to this rule is mapping to the temporary mapping page
    // table since this is private per-cpu.
    ASSERT(addr_space == get_kernel_addr_space() ||
           is_user_addr(vaddr) ||
           is_temp_mapping(vaddr));

    struct page_dir * const page_dir = get_page_dir(addr_space);

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
        if (new_table == NO_FRAME) {
            SET_ERROR("Cannot allocate new page table", ENONE);
            return false;
        }
        page_table_allocated = true;
        union pde_t pde;
        // Try to be as inclusive as possible for the PDE. The PTEs will
        // enforced the actual attributes.
        pde.writable = 1;
        pde.write_through = 0;
        pde.cache_disable = 0;
        // Reset accessed bit.
        pde.accessed = 0;

        pde.user_accessible = (bool)(flags & VM_USER);

        pde.page_table_addr = ((uint32_t)new_table) >> 12;
        pde.present = 1;

        // The same page table can be used for both mappings.
        page_dir->entry[pde_idx] = pde;
    }

    struct page_table * const page_table = get_page_table(page_dir, pde_idx);
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
    return true;
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

    struct addr_space * const addr_space = get_curr_addr_space();

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
        if (!map_page_in(addr_space, paddr, vaddr, flags)) {
            PANIC("Cannot create higher half mapping\n");
        }
        // Identity map the low addresses.
        if (!map_page_in(addr_space, paddr, paddr, flags)) {
            PANIC("Cannot create identity mapping\n");
        }
    }
}

// Initialize and enable paging. After calling this function the processor uses
// the higher-half kernel (EIP, stack and GDTR point to the higher-half).
// @param esp: The stack pointer value right before calling this function. This
// parameter is important as it allows us to fixup the return address pushed on
// the stack as a _physical_ address.
void init_paging(void) {
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
    
    LOG("Initializing paging.\n");

    // Allocate a page directory for the kernel itself. Use the to_virt() macro
    // so that we can manipulate the page directory directly.
    struct page_dir * const page_dir_phy = alloc_page_dir();
    if (page_dir_phy == NO_FRAME) {
        PANIC("Not enough physical memory to even initialize paging ??\n");
    }
    LOG("Kernel's page directory allocated at phy address %p\n", page_dir_phy);

    struct page_dir * const page_dir = to_virt(page_dir_phy);
    memzero(page_dir, PAGE_SIZE);

    // Initialize the kernel's struct addr_space with the frame we just
    // allocated for the page directory. This _must_ be done before calling any
    // mapping/unmapping function as those will use the struct addr_space.
    init_kernel_addr_space(page_dir_phy);

    // Switch to the kernel's address space. Do this now so that the percpu var
    // curr_addr_space is set and the code can call get_curr_addr_space().
    LOG("Setting CR3 to kernel page directory.\n");
    switch_to_addr_space(get_kernel_addr_space());

    // Create both mappings.
    LOG("Creating ID and higher half mappings.\n");
    create_identity_and_higher_half_mappings(page_dir);

    // Set the last entry in the page directory to point to itself. This is to
    // implement recursive page tables which makes it easier to modify page
    // directories and page tables once paging is enabled.
    LOG("Creating recursive entry in kernel page directory.\n");
    create_recursive_entry(page_dir);

    // Create a special page table that will be used by cpus to create temporary
    // mappings (ex: modifying other page directory, read/write in physical
    // memory).
    LOG("Creating temporary mapping page table in kernel page directory.\n");
    create_temp_mapping_entry(page_dir);

    // Pre-allocate all the page tables used for kernel space. We are doing that
    // in order to avoid allocating page tables for the kernel in the future
    // when mapping to kernel memory. This is because doing so would required
    // some sort of sync between ALL page directories to make sure that they all
    // have the same PDEs.
    LOG("Preallocating kernel space page tables.\n");
    preallocate_kernel_page_table(page_dir);

    // Enable the paging bit.
    LOG("Enabling paging bit. CR3 = %p\n", cpu_read_cr3());
    cpu_enable_paging();

    LOG("Paging enabled, EIP = %p.\n", cpu_read_eip());

    // Since we are now implementing higher half mapping using paging, we can
    // change the GDT entries of the Boot GDT to become flat segments.
    fixup_gdt_after_paging_enable();

    // The value of the IDTR on the current processor needs to be updated to an
    // higher half linear address.
    interrupt_fixup_idtr();

    // We can now get rid of the identity mapping.
    LOG("Getting rid of ID mapping.\n");
    remove_identity_mapping();
    cpu_invalidate_tlb();
}

// Check if a page table is empty, that is all of its entry have the present bit
// set to 0.
// @param table: The page table to test.
// @return: true if the page table is empty, false otherwise.
static bool page_table_is_empty(struct page_table const * const table) {
    for (uint16_t i = 0; i < PTES_PER_PAGE; ++i) {
        if (table->entry[i].present) {
            return false;
        }
    }
    return true;
}

// Unmap a virtual page.
// @param addr_space: The address space in which the unmapping should be
// performed.
// @param vaddr: The address of the virtual page. Must be 4Kib aligned.
// @param free_phy_frame: If set to true, the physical frame pointed by vaddr
// will be freed.
static void unmap_page_in(struct addr_space * const addr_space,
                          void const * const vaddr,
                          bool const free_phy_frame) {
    ASSERT(is_4kib_aligned(vaddr));
    // Since the address space of processes are sharing the kernel address, if
    // one modify kernel mappings with any address space then the lock will not
    // protect against race condition. Hence we enforce the rule: Modifying a
    // kernel mapping can only be done in the kernel address space, not from a
    // process' address space.
    ASSERT(addr_space == get_kernel_addr_space() || (is_user_addr(vaddr)));

    struct page_dir * page_dir = get_page_dir(addr_space);

    uint32_t const pde_idx = pde_index(vaddr);
    if (!page_dir->entry[pde_idx].present) {
        PANIC("Address %p is not mapped.", vaddr);
    }

    struct page_table * const page_table = get_page_table(page_dir, pde_idx);
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

        // We need to recall get_page_dir() here. This is because if we are
        // operating in another address space than the current, get_page_dir()
        // had used a temp mapping to access the page dir, and that temp mapping
        // has been overwritten by get_page_table().
        page_dir = get_page_dir(addr_space);
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
// @param addr_space: The address space in which the mapping should be added.
// @param paddr: The physical address to map the virtual address to.
// @param vaddr: The virtual address.
// @param len: The length in byte of the memory region. Note that mapping should
// ideally be multiple of PAGE_SIZE (since that is the granularity), but this
// field does not have to be.
// @param flags: The attributes of the mapping. See VM_* macros in paging.h.
// Note: The addresses do not have to be 4KiB aligned, the mapping function will
// take care of that. However they need to have the same page offset (lower 12
// bits).
// @return: true if the mapping was succesful, false otherwise. If this
// function returns false then the paging structure/addr space is guaranteed to
// be untouched.
// Note: This function assumes that the virtual address space is locked.
static bool do_paging_map_in(struct addr_space * const addr_space,
                             void const * const paddr,
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
        if (!map_page_in(addr_space, pchunk, vchunk, flags)) {
            // The mapping failed because we ran out of memory to allocate a new
            // page table. Undo all the mapped frames above and return failure.
            SET_ERROR("Failed to map one frame in request", ENONE);
            for (size_t j = 0; j < i; ++j) {
                unmap_page_in(addr_space, start_virt + j * PAGE_SIZE, false);
            }
            return false;
        }
    }
    return true;
}

bool paging_map_in(struct addr_space * const addr_space,
                   void const * const paddr,
                   void const * const vaddr,
                   size_t const len,
                   uint32_t const flags) {
    spinlock_lock(&addr_space->lock);
    bool const res = do_paging_map_in(addr_space, paddr, vaddr, len, flags);
    spinlock_unlock(&addr_space->lock);

    if (res) {
        cpu_invalidate_tlb();
        maybe_to_tlb_shootdown();
    }
    return res;
}

// Unmap a virtual memory region.
// @param addr_space: The address space in which the mapping should be removed.
// @param vaddr: The virtual address to unmap.
// @param len: The length of the memory area.
// @param free_phy_frame: If true, the physical frame mapped to the memory
// region will be freed.
static void do_paging_unmap_in(struct addr_space * const addr_space,
                               void const * const vaddr,
                               size_t const len,
                               bool const free_phy_frame) {
    void const * const start_virt = get_page_addr(vaddr);
    size_t const fixed_len = len + (uint32_t)(vaddr - start_virt);

    uint32_t const num_frames = ceil_x_over_y_u32(fixed_len, PAGE_SIZE);
    for (size_t i = 0; i < num_frames; ++i) {
        unmap_page_in(addr_space, start_virt + i * PAGE_SIZE, free_phy_frame);
    }
}

void paging_unmap_in(struct addr_space * const addr_space,
                     void const * const vaddr,
                     size_t const len) {
    spinlock_lock(&addr_space->lock);
    do_paging_unmap_in(addr_space, vaddr, len, false);
    spinlock_unlock(&addr_space->lock);

    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();
}

void paging_unmap_and_free_frames_in(struct addr_space * const addr_space,
                                     void const * const vaddr,
                                     size_t const len) {
    lock_addr_space(addr_space);
    do_paging_unmap_in(addr_space, vaddr, len, true);
    unlock_addr_space(addr_space);

    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();
}

// Check if a virtual page is currently mapped to a frame in physical memory.
// @param addr_space: The address to check.
// @param vaddr: The address (virtual) of the page. This address should be 4KiB
// aligned.
// @return: true if vaddr is mapped, false otherwise.
static bool page_is_mapped(struct addr_space * const addr_space,
                           void const * const vaddr) {
    ASSERT(is_4kib_aligned(vaddr));
    struct page_dir * const page_dir = get_page_dir(addr_space);
    uint32_t const pde_idx = pde_index(vaddr);
    uint32_t const pte_idx = pte_index(vaddr);
    if (!page_dir->entry[pde_idx].present) {
        return false;
    }
    struct page_table * const table = get_page_table(page_dir, pde_idx);
    bool const mapped = table->entry[pte_idx].present;

    return mapped;
}

// Find the next non-mapped page in an address space.
// @param addr_space: The address space in which the search should be performed.
// @param start: The page address to start searching from.
// @return: The address of the next non mapped page. If no such page is found,
// NO_REGION is returned.
static void * find_next_non_mapped_page(struct addr_space * const addr_space,
                                        void * const start) {
    void * ptr = start;
    // We can only search up to the address which is the first address of the
    // temp mapping page table.
    void * const max_ptr = (void*)(TEMP_MAP_PDE_IDX << 22);
    while (ptr < max_ptr) {
        if (!page_is_mapped(addr_space, ptr)) {
            return ptr;
        }
        ptr += PAGE_SIZE;
    }
    return NO_REGION;
}

// Compute the size of the hole (aka non mapped memory) starting at a virtual
// page address.
// @param addr_space: The address space to use.
// @param vaddr: The start address to compute the size of the hole from.
// @return: The size of the hole in number of pages.
static uint32_t compute_hole_size(struct addr_space * const addr_space,
                                  void const * const vaddr) {
    struct page_dir * const page_dir = get_page_dir(addr_space);
    uint32_t const start_pde_idx = pde_index(vaddr);
    uint32_t count = 0;

    // Recursive PDE entry and temp mapping PDE entry cannot be part of the
    // hole.
    for (uint16_t i = start_pde_idx; i < PDES_PER_PAGE - 2; ++i) {
        union pde_t const * const pde = &page_dir->entry[i];
        if (!pde->present) {
            if (i == start_pde_idx) {
                count += PTES_PER_PAGE - pte_index(vaddr);
            } else {
                count += PTES_PER_PAGE;
            }
            continue;
        }

        struct page_table * const page_table = get_page_table(page_dir,i);
        uint16_t const start_pte_idx = i==start_pde_idx ? pte_index(vaddr) : 0;
        for (uint16_t j = start_pte_idx; j < PTES_PER_PAGE; ++j) {
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
// @param addr_space: The address space in which the search should be carried
// out.
// @param start_addr: The start address to search from.
// @parma npages: The minimum number of contiguous non mapped pages to look for.
// @return: The start virtual address of the memory region.
static void *do_paging_find_contiguous_non_mapped_pages_in(
    struct addr_space * const addr_space,
    void * const start_addr,
    size_t const npages) {

    void * ptr = find_next_non_mapped_page(addr_space, start_addr);
    while (ptr != NO_REGION) {
        uint32_t const hole = compute_hole_size(addr_space, ptr);
        if (hole >= npages) {
            return ptr;
        }
        ptr = find_next_non_mapped_page(addr_space, ptr + PAGE_SIZE);
    }

    // Could not find a hole big enough.
    SET_ERROR("Could not find big enough region in virt memory space", ENOMEM);
    return NO_REGION;
}

void *paging_find_contiguous_non_mapped_pages_in(
    struct addr_space * const addr_space,
    void * const start_addr,
    size_t const npages) {

    lock_addr_space(addr_space);
    void * const res = do_paging_find_contiguous_non_mapped_pages_in(addr_space,
                                                                     start_addr,
                                                                     npages);
    unlock_addr_space(addr_space);
    return res;
}

void *paging_map_frames_above_in(struct addr_space * const addr_space,
                                 void * const start_addr,
                                 void ** frames,
                                 size_t const npages,
                                 uint32_t const flags) {
    lock_addr_space(addr_space);
    void * const start = do_paging_find_contiguous_non_mapped_pages_in(
        addr_space, start_addr, npages);
    if (start == NO_REGION) {
        // Cannot find a region big enough to map all the frames to.
        unlock_addr_space(addr_space);
        return NO_REGION;
    }

    for (size_t i = 0; i < npages; ++i) {
        void const * const frame = frames[i];
        bool const res = do_paging_map_in(addr_space,
                                          frame,
                                          start + i * PAGE_SIZE,
                                          PAGE_SIZE,
                                          flags);
        if (!res) {
            // This is not exactly what happens but it is better for the caller
            // to deal with a single error return value than 2.
            SET_ERROR("Could not map frames in virtual mem space hole", ENONE);
            return NO_REGION;
        }
    }
    unlock_addr_space(addr_space);

    // TLB invalidation must be done outside the critical section.
    cpu_invalidate_tlb();
    maybe_to_tlb_shootdown();

    return start;
}

void paging_setup_new_page_dir(void * const page_dir_phy_addr) {
    // Copy each entry in the new page directory.
    struct page_dir const * const curr_pd = get_page_dir(get_curr_addr_space());
    struct page_dir * const dest_pd = create_temp_mapping(page_dir_phy_addr);

    // Getting a pointer on the page dir of the current address space should not
    // use a temp mapping but the recursive entry instead.
    ASSERT(curr_pd != dest_pd);

    memzero(dest_pd, PAGE_SIZE);

    uint16_t const start_idx = pde_index(KERNEL_PHY_OFFSET);
    for (uint16_t i = start_idx; i < PDES_PER_PAGE - 1; ++i) {
        dest_pd->entry[i] = curr_pd->entry[i];
    }

    // Setup the temporary mapping page table. Nothing to change here, all cpus
    // are using the same page table.
    dest_pd->entry[TEMP_MAP_PDE_IDX] = curr_pd->entry[TEMP_MAP_PDE_IDX];

    // The last PDE is the recursive entry. This one cannot be simply copied as
    // it needs to point to the frame of the page directory. To make it simple,
    // take the recursive entry of the kernel page dir and simply change the
    // page_table_addr field instead of setting all the fields manually.
    union pde_t rec = curr_pd->entry[RECURSIVE_PDE_IDX];

    // Sanity check on the current address space's recursive entry;
    void * const curr_pd_addr = get_curr_addr_space()->page_dir_phy_addr;
    ASSERT(rec.page_table_addr == (uint32_t)curr_pd_addr >> 12);

    // Keep everything the same except the address where the entry points to.
    rec.page_table_addr = ((uint32_t)page_dir_phy_addr) >> 12;
    dest_pd->entry[RECURSIVE_PDE_IDX] = rec;
}

void paging_free_addr_space(struct addr_space * const addr_space) {
    // Don't touch at kernel page tables.
    uint16_t const max_index = pde_index(KERNEL_PHY_OFFSET);
    for (uint16_t i = 0; i < max_index; ++i) {
        // We need to recall get_page_dir() here. This is because if we are
        // operating in another address space than the current, get_page_dir()
        // had used a temp mapping to access the page dir, and that temp mapping
        // has been overwritten by get_page_table().
        struct page_dir * const page_dir = get_page_dir(addr_space);
        union pde_t pde = page_dir->entry[i];
        if (!pde.present) {
            continue;
        }

        struct page_table * const page_table = get_page_table(page_dir, i);

        // Free any frame referenced by this page table.
        for (uint16_t j = 0; j < PTES_PER_PAGE; ++j) {
            union pte_t pte = page_table->entry[j];
            if (!pte.present) {
                continue;
            }
            void * const frame = (void*)(pte.frame_addr << 12);
            free_frame(frame);
        }

        // Free the physical frame used to hold the page table.
        void * const frame = (void*)(pde.page_table_addr << 12);
        free_frame(frame);
    }

    // Free the physical frame used for the page dir.
    free_frame(addr_space->page_dir_phy_addr);

    maybe_to_tlb_shootdown();
}

#include <paging.test>
