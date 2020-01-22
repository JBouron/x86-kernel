#include <multiboot.h>
#include <cpu.h>
#include <kernel_map.h>
#include <debug.h>
#include <math.h>

// Some helper functions to interact with the multiboot header.

// This global hold the physical address of the mutliboot info structure in RAM.
// Defaults to NULL and is initialized in init_multiboot.
static struct multiboot_info const * __MULTIBOOT_INFO = NULL;
// Use a "generic" pointer that works with both physical and virtual addressing.
#define MULTIBOOT_INFO (*(PTR(__MULTIBOOT_INFO)))

// This tables contain ranges of physical memory that is reserved. This is
// required because the memory map given by the bootloader possesses some
// shortcomings:
//   * It does not marked the physical memory where the kernel has been loaded
//   to as non available.
//   * The same applies for some hardware (eg. VGA buffer).
//   * The memory containing the multiboot structure (and other data) is marked
//   available as well.
// Naively traversing the memory map to find "free" memory won't work, we need
// to be aware of those three things above.
// The table below contains multiple pairs of addresses (low, high) where low is
// the address of the lowest byte reserved, and high the one of the higher byte
// reserved.
// The table is initialized in the init_reserved_memory_area function called
// during multiboot initialization.
#define NUM_RESERVED_MEM    3
static uint32_t __RESERVED_MEM[NUM_RESERVED_MEM][2];

// Because the table above might be access both before and after paging gets
// enabled, it is necessary to compute the right pointer to it. This macro
// provides a shortcut to be sure that the correct address is used. It should be
// treated as the global itself.
#define RESERVED_MEM (*(PTR(__RESERVED_MEM)))

// Initialize the RESERVED_MEM table.
static void init_reserved_memory_area(void) {
    // The kernel.
    RESERVED_MEM[0][0] = (uint32_t)to_phys(KERNEL_START_ADDR);
    RESERVED_MEM[0][1] = (uint32_t)to_phys(KERNEL_END_ADDR);

    // The multiboot structure.
    uint32_t const start_mb = min_u32((uint32_t)MULTIBOOT_INFO,
        MULTIBOOT_INFO->mmap_addr);
    uint32_t const end_mb = max_u32((uint32_t)MULTIBOOT_INFO +
        sizeof(*MULTIBOOT_INFO) - 1, MULTIBOOT_INFO->mmap_addr +
        MULTIBOOT_INFO->mmap_length);
    RESERVED_MEM[1][0] = start_mb;
    RESERVED_MEM[1][1] = end_mb;

    // The VGA buffer. TODO: Remove the hardcoded address here.
    uint32_t const vga_buf_offset = 0xB8000;
    uint32_t const vga_bug_len = 80 * 25 * 2;
    RESERVED_MEM[2][0] = vga_buf_offset;
    RESERVED_MEM[2][1] = vga_buf_offset + vga_bug_len - 1;
}

void init_multiboot(struct multiboot_info const * const ptr) {
    ASSERT(!cpu_paging_enabled());
    // Set the global variable containing the physical pointer to the multiboot
    // info.
    MULTIBOOT_INFO = ptr;
    // Initialize the array containing the reserved physical memory areas.
    init_reserved_memory_area();
}

// Get a pointer on the multiboot structure in memory.
// @return: A physical pointer if paging is not enabled, a virtual pointer
// otherwise.
static struct multiboot_info const * get_multiboot(void) {
    if (cpu_paging_enabled()) {
        // The global MULTIBOOT_INFO contains the physical address of the
        // multiboot header. Translate it to a virtual address. This assumes
        // that the lower 1MiB is mapped to KERNEL_PHY_OFFSET as well.
        return to_virt(MULTIBOOT_INFO);
    } else {
        // Since paging is not yet enabled we need to use the physical address
        // of the global variable.
        return MULTIBOOT_INFO;
    }
}

struct multiboot_info const *get_multiboot_info_struct(void) {
    return get_multiboot();
}

struct multiboot_mmap_entry const *get_mmap_entry_ptr(void) {
    struct multiboot_info const * mi = get_multiboot();
    ASSERT(mi->mmap_addr);
    return (struct multiboot_mmap_entry*)mi->mmap_addr;
}

uint32_t multiboot_mmap_entries_count(void) {
    struct multiboot_info const * mi = get_multiboot();
    ASSERT(mi->mmap_length % sizeof(struct multiboot_mmap_entry) == 0);
    return mi->mmap_length / sizeof(struct multiboot_mmap_entry);
}

bool mmap_entry_is_available(struct multiboot_mmap_entry const * const entry) {
    // A type of value 1 indicates that the entry references RAM that is
    // available for use.
    return entry->type == 1;
}

bool mmap_entry_within_4GiB(struct multiboot_mmap_entry const * const entry) {
    return entry->base_addr <= 0xFFFFFFFF;
}

// Get the offset of the last byte of a memory map entry.
// @param entry: The entry to get the max offset from.
// @return: The (physical) offset of the last byte of the entry.
static uint64_t get_max_offset_for_entry(
    struct multiboot_mmap_entry const * const entry) {
    ASSERT(mmap_entry_is_available(entry));
    return entry->base_addr + entry->length - 1;
}

void * get_max_addr_for_entry(struct multiboot_mmap_entry const * const ent) {
    return (void*)(uint32_t)get_max_offset_for_entry(ent);
}

void *get_max_addr(void) {
    // The goal here is to find the maximum address in RAM by traversing the
    // memory map provided by the multiboot info structure.
    // The max address is the address of the last byte of the last available
    // region.
    // Note that since this kernel does not support more than 4GiB of RAM we
    // will limit ourselves at the 4GiB limit.

    struct multiboot_mmap_entry const * const first = get_mmap_entry_ptr();
    uint32_t const count = multiboot_mmap_entries_count();

    // Go through the mmap buffer and look for the last available range of RAM.
    struct multiboot_mmap_entry const * entry = first;
    uint64_t curr_max_addr = 0;
    while (entry < first + count) {
        if (mmap_entry_is_available(entry) && mmap_entry_within_4GiB(entry)) {
            // Skip the entrie that are above 4GiB.
            uint64_t const entry_max = get_max_offset_for_entry(entry);
            if (entry_max > curr_max_addr) {
                curr_max_addr = entry_max;
            }
        }
        ++entry;
    }

    // It may happen that the entry starts at an address < 4GiB but is big
    // enough to reach over the 4GiB limit. If that's the case truncate to 4GiB.
    if (curr_max_addr > 0xFFFFFFFF) {
        curr_max_addr = 0xFFFFFFFF;
    }

    // The cast to uint32_t is safe here as curr_max_addr cannot be over the
    // uint32_t max value.
    return (void*)(uint32_t)curr_max_addr;
}

// Check if an entry contains any reserved memory as specified in the
// RESERVED_MEM table.
// @param entry: The entry to test.
// @param index [out]: The index in the RESERVED_MEM of the reserved memory
// region conflicting with the entry. If this pointer is NULL then it is
// ignored.
// @return: true if the entry contains any reserved memory, false otherwise.
// When returning true, the index pointer will contain the index, in the
// RESERVED_MEM of the conflicting region, otherwise it is untouched.
static bool contain_reserved_memory(
    struct multiboot_mmap_entry const * const entry,
    uint32_t * const index) {

    if (!entry->length) {
        // This is an empty entry, this might happen in the recursion in
        // find_in_entry. In this case the entry does not contain any reserved
        // memory.
        return false;
    }

    // Compute the start and end offsets of the entry.
    uint64_t const entry_start = entry->base_addr;
    uint64_t const entry_end = get_max_offset_for_entry(entry);

    // Check the RESERVED_MEM table for any conflict.
    for (uint32_t i = 0; i < NUM_RESERVED_MEM; ++i) {
        uint64_t const reserved_start = RESERVED_MEM[i][0];
        uint64_t const reserved_end = RESERVED_MEM[i][1];
        if (!((entry_start < reserved_start && entry_end < reserved_start) ||
            (reserved_end < entry_start && reserved_end < entry_end))) {
            // This entry conflict with the ith element of the reserved memory
            // table. Set the index pointer and return.
            if (index) {
                *index = i;
            }
            return true;
        }
    }
    
    // Couldn't find any conflict.
    return false;
}

// Check if an entry contains contiguous frames. This is an helper function for
// find_contiguous_physical_frames.
// @param entry: The entry to test.
// @param nframes: The number of desired contiguous frames.
// @param start [out]: If the amount of memory is available, this pointer will
// contain the start address of the memory region. This address is 4KiB aligned.
// @return: true if the entry contains the requested amount of available,
// contiguous memory, false otherwise. If this function returns true then
// `start` will contain the start address, otherwise it is untouched.
static bool find_in_entry(struct multiboot_mmap_entry const * const entry,
                          size_t const nframes,
                          void **start) {
    uint32_t reserved_mem_index = 0;
    if (!mmap_entry_within_4GiB(entry)) {
        return false;
    }
    else if (contain_reserved_memory(entry, &reserved_mem_index)) {
        // This entry overlap with the ith reserved memory region from the
        // RESERVED_MEM table. Check if we can still fit nframes around the
        // reserved region.

        // Compute the start and end offsets of the reserved memory region as
        // well as the entry itself.
        uint64_t const rstart = RESERVED_MEM[reserved_mem_index][0];
        uint64_t const rend = RESERVED_MEM[reserved_mem_index][1];
        uint64_t const estart = entry->base_addr;
        uint64_t const eend = get_max_offset_for_entry(entry);

        if (estart <= rstart) {
            // There is available memory between the start of the entry and the
            // start of the reserved memory, check if it is big enough to fit
            // the requested number of frame.
            struct multiboot_mmap_entry before = {
                .base_addr = estart,
                .length = rstart - estart,
                .type = 1,
            };
            // Make sure that the recursion does not go deeper.
            ASSERT(!contain_reserved_memory(&before, NULL));
            if (find_in_entry(&before, nframes, start)) {
                return true;
            }
        }

        if (rend <= eend) {
            // Now do the same for the potential available memory between the
            // end of the reserved memory region and the end of the entry.
            struct multiboot_mmap_entry after = {
                .base_addr = rend + 1,
                .length = eend - rend,
                .type = 1,
            };
            // Avoid deep recursions.
            ASSERT(!contain_reserved_memory(&after, NULL));
            if (find_in_entry(&after, nframes, start)) {
                return true;
            }
        }

        // None of the holes (if any) could contain the requested amount,
        // nothing we can do with this entry.
        return false;
    } else {
        // We know the base addr is within the 4GiB therefore the cast to
        // uint32_t is safe here.
        uint64_t const aligned = round_up_u32(entry->base_addr, 0x1000);
        uint64_t const new_len = entry->length - (aligned - entry->base_addr);

        if (new_len < nframes * 0x1000) {
            // Aligning the address to a page does not have enough space to fit
            // the memory area.
            return false;
        } else if (aligned + new_len > 0xFFFFFFFF) {
            // We reached the 4GiB limit, this entry won't work.
            return false;
        } else {
            // This entry will work.
            *start = (void*) (uint32_t) aligned;
            return true;
        }
    }
}

// Find in RAM a certain number of contiguous frames.
// @param nframes: The number of desired contiguous frames.
// @return: The start address of the area. This address is 4KiB aligned.
// Note: If no such area is available, this function will trigger a kernel
// panic. The rational here is that this function is used to allocate the
// physical memory that will contain the bitmap of free and used physical
// frames. If we can't even allocate this bitmap then there is no point going
// further.
void * find_contiguous_physical_frames(size_t const nframes) {
    struct multiboot_mmap_entry const * const first = get_mmap_entry_ptr();
    uint32_t const count = multiboot_mmap_entries_count();

    struct multiboot_mmap_entry const * entry = first;
    while (entry < first + count) {
        if (mmap_entry_is_available(entry) &&
            entry->length >= nframes * 0x1000) {
            void * start = NULL;
            if (find_in_entry(entry, nframes, &start)) {
                return start;
            }
        }
        ++entry;
    }
    PANIC("Not enough physical memory to contain %u contiguous frame", nframes);
    // Unreachable.
    return NULL;
}

#include <multiboot.test>
