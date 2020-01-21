#include <multiboot.h>
#include <cpu.h>
#include <kernel_map.h>
#include <debug.h>
#include <math.h>

// Some helper functions to interact with the multiboot header.

// This global hold the physical address of the mutliboot info structure in RAM.
// Defaults to NULL and is initialized in init_multiboot.
static struct multiboot_info const * MULTIBOOT_INFO = NULL;

void init_multiboot(struct multiboot_info const * const ptr) {
    ASSERT(!cpu_paging_enabled());
    // Since paging is not yet enabled we need to use the physical address of
    // the global variable to write it.
    struct multiboot_info const ** const global_ptr = to_phys(&MULTIBOOT_INFO);
    *global_ptr = ptr;
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
        return *(struct multiboot_info const **)to_phys(&MULTIBOOT_INFO);
    }
}

struct multiboot_info const *get_multiboot_info_struct(void) {
    return get_multiboot();
}

struct multiboot_mmap_entry const *get_mmap_entry_ptr(void) {
    struct multiboot_info const * mi = get_multiboot();
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

// Get the maximum address from a memory map entry.
// @param entry: The entry to get the maximum address from.
// @return: The physical address of the very last byte of the entry.
static uint64_t get_max_addr_for_entry(
    struct multiboot_mmap_entry const * const entry) {
    ASSERT(mmap_entry_is_available(entry));
    return entry->base_addr + entry->length - 1;
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
            uint64_t const entry_max = get_max_addr_for_entry(entry);
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

// Check if an entry contains parts or the entire kernel in its memory range.
// @param entry: The entry to test.
// @return: true if the entry contain any byte of the kernel.
static bool contains_kernel(struct multiboot_mmap_entry const * const entry) {
    uint64_t const kernel_start = (uint32_t)to_phys(KERNEL_START_ADDR);
    uint64_t const kernel_end = (uint32_t)to_phys(KERNEL_END_ADDR);
    uint64_t const entry_start = entry->base_addr;
    uint64_t const entry_end = get_max_addr_for_entry(entry);

    return !((entry_start < kernel_start && entry_end < kernel_start) ||
        (kernel_end < entry_start && kernel_end < entry_end));
}

// Check if an entry contains contiguous availale memory.
// @param entry: The entry to test.
// @param len: The length desired of contiguous available memory.
// @param start [out]: If the amount of memory is available, this pointer will
// contain the start address of the memory region. This address is 4KiB aligned.
// @return: true if the entry contains the requested amount of available,
// contiguous memory, false otherwise. If this function returns true then
// `start` will contain the start address, otherwise it is untouched.
static bool find_in_entry(struct multiboot_mmap_entry const * const entry,
                          size_t const len,
                          void **start) {
    if (!mmap_entry_within_4GiB(entry)) {
        return false;
    }
    else if (contains_kernel(entry)) {
        // Since the kernel is in this entry we need to check the holes
        // before and after the kernel (if any).
        uint64_t const kstart = (uint32_t)to_phys(KERNEL_START_ADDR);
        uint64_t const kend = (uint32_t)to_phys(KERNEL_END_ADDR);
        uint64_t const estart = entry->base_addr;
        uint64_t const eend = get_max_addr_for_entry(entry);

        if (estart <= kstart) {
            // We have a hole before the kernel, check it.
            struct multiboot_mmap_entry before = {
                .base_addr = estart,
                .length = kstart - estart,
                .type = 1,
            };
            // Avoid ending in an infinite recursion.
            ASSERT(!contains_kernel(&before));
            if (find_in_entry(&before, len, start)) {
                return true;
            }
        }

        if (kend <= eend) {
            // We have a hole after the kernel, check it.
            struct multiboot_mmap_entry after = {
                // The base address is the address of the first byte right after
                // the kernel.
                .base_addr = kend + 1,
                .length = eend - kend,
                .type = 1,
            };
            // Avoid ending in an infinite recursion.
            ASSERT(!contains_kernel(&after));
            if (find_in_entry(&after, len, start)) {
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

        if (new_len < len) {
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

// Find a physcial memory area in RAM of a given size.
// @param len: The length in bytes of the request.
// @return: The start address of the area. This address is 4KiB aligned.
// Note: If no such area is available, this function will trigger a kernel
// panic. The rational here is that this function is used to allocate the
// physical memory that will contain the bitmap of free and used physical
// frames. If we can't even allocate this bitmap then there is no point going
// further.
void * find_memory_area(size_t const len) {
    struct multiboot_mmap_entry const * const first = get_mmap_entry_ptr();
    uint32_t const count = multiboot_mmap_entries_count();

    struct multiboot_mmap_entry const * entry = first;
    while (entry < first + count) {
        if (mmap_entry_is_available(entry) && entry->length >= len) {
            void * start = NULL;
            if (find_in_entry(entry, len, &start)) {
                return start;
            }
        }
        ++entry;
    }
    PANIC("Not enough physical memory to contain %u contiguous bytes", len);
    // Unreachable.
    return NULL;
}

#include <multiboot.test>
