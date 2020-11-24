#pragma once
#include <types.h>
#include <addr_space.h>

#define PAGE_SIZE   0x1000

// Initialize and enable paging.
void init_paging(void);

// Allow writes on the page.
#define VM_WRITE            (1 << 0)
// Use write-trough on the page.
#define VM_WRITE_THROUGH    (1 << 1)
// Disable cache for this page.
#define VM_CACHE_DISABLE    (1 << 2)
// Allow user access to this page.
#define VM_USER             (1 << 3)
// Do not set the global bit for the mapping of this page.
#define VM_NON_GLOBAL       (1 << 4)

// Map a virtual memory region to a physical one.
// @param addr_space: The address space in which the mapping will be performed.
// @param paddr: The physical address to map the virtual address to. Must be
// 4KiB aligned.
// @param vaddr: The virtual address. Must be 4KiB aligned.
// @param len: The length in byte of the memory region. Note that mapping should
// ideally be multiple of PAGE_SIZE (since that is the granularity), but this
// field does not have to be.
// @param flags: The attributes of the mapping. See VM_* macros in paging.h.
// @return: true if the mapping was successful, false otherwise.
bool paging_map_in(struct addr_space * const addr_space,
                   void const * const paddr,
                   void const * const vaddr,
                   size_t const len,
                   uint32_t const flags);

// Map a virtual memory region to a physical one in the current address space.
// @param paddr: The physical address to map the virtual address to. Must be
// 4KiB aligned.
// @param vaddr: The virtual address. Must be 4KiB aligned.
// @param len: The length in byte of the memory region. Note that mapping should
// ideally be multiple of PAGE_SIZE (since that is the granularity), but this
// field does not have to be.
// @param flags: The attributes of the mapping. See VM_* macros in paging.h.
#define paging_map(paddr, vaddr, len, flags) \
    paging_map_in(get_curr_addr_space(), (paddr), (vaddr), (len), (flags))

// Unmap a virtual memory region.
// @param addr_space: The address space in which the unmapping will be
// performed.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
void paging_unmap_in(struct addr_space * const addr_space,
                     void const * const vaddr,
                     size_t const len);

// Unmap a virtual memory region in the current address space.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
#define paging_unmap(vaddr, len) \
    paging_unmap_in(get_curr_addr_space(), (vaddr), (len))

// Unmap a virtual memory region and free the physical frames it was mapped to.
// @param addr_space: The address space in which the unmapping will be
// performed.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
void paging_unmap_and_free_frames_in(struct addr_space * const addr_space,
                                     void const * const vaddr,
                                     size_t const len);

// Unmap a virtual memory region in the current address space and free the
// physical frames it was mapped to.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
#define paging_unmap_and_free_frames(vaddr, len) \
    paging_unmap_and_free_frames_in(get_curr_addr_space(), (vaddr), (len))

// Special value returned by paging_find_contiguous_non_mapped_pages(_in) and
// paging_map_frames_above(_in) in case there is no hole big enough in the
// virtual address space to contain the requested amount of memory.
#define NO_REGION   (void*)(-1UL)

// Find a memory region in the an address space with at least a certain number
// of contiguous pages non mapped.
// @param addr_space: The address space to look into.
// @param start_addr: The start address to search from.
// @parma npages: The minimum number of contiguous non mapped pages to look for.
// @return: The start virtual address of the memory region. If no such region
// can be found (memory space full) this function return NO_REGION.
void *paging_find_contiguous_non_mapped_pages_in(
    struct addr_space * const addr_space,
    void * const start_addr,
    size_t const npages);

// Find a memory region in the current virtual address space with at least a
// certain number of contiguous pages non mapped.
// @param start_addr: The start address to search from.
// @parma npages: The minimum number of contiguous non mapped pages to look for.
// @return: The start virtual address of the memory region. If no such region
// can be found (memory space full) this function return NO_REGION.
#define paging_find_contiguous_non_mapped_pages(start_addr, npages)     \
    paging_find_contiguous_non_mapped_pages_in(get_curr_addr_space(),   \
                                               (start_addr),    \
                                               (npages))

// Maps physical frames to virtual memory above a certain address. The mapping
// will be continous in the virtual address space. Notes: The frames will not
// necessarily mapped at `start_addr` exactly but this function guarantees it
// will be mapped to an address >= start_addr.
// @param addr_space: The address space to create the mapping into.
// @param start_addr: The min virtual address to map the frames to.
// @param frames: An array containing the physical frames to be mapped to
// virtual memory.
// @param npages: The number of physical frames to mapped.
// @param flags: The flags to use when mapping the physical frames.
// @return: The start virtual address of the memory region. If the function
// cannot find a region in the address space big enough to contain the requested
// number of pages it returns NO_REGION and nothing will be mapped. NO_REGION
// will also be returned if this function fails to map the frames even though
// the space in the virtual address space exists.
void *paging_map_frames_above_in(struct addr_space * const addr_space,
                                 void * const start_addr,
                                 void ** frames,
                                 size_t const npages,
                                 uint32_t const flags);

// Maps physical frames to virtual memory above a certain address. The mapping
// will be continous in the virtual address space. Notes: The frames will not
// necessarily mapped at `start_addr` exactly but this function guarantees it
// will be mapped to an address >= start_addr.
// The mapping will be performed in the current address space.
// @param start_addr: The min virtual address to map the frames to.
// @param frames: An array containing the physical frames to be mapped to
// virtual memory.
// @param npages: The number of physical frames to mapped.
// @param flags: The flags to use when mapping the physical frames.
// @return: The start virtual address of the memory region. If the function
// cannot find a region in the address space big enough to contain the requested
// number of pages it returns NO_REGION and nothing will be mapped.
#define paging_map_frames_above(start_addr, frames, npages, flags)  \
    paging_map_frames_above_in(get_curr_addr_space(),               \
                               (start_addr),                        \
                               (frames),                            \
                               (npages),                            \
                               (flags))

// Setup a new page directory. This function will initialized the page directory
// with the entries used by the kernel as well as the recursive entry. Any
// address below KERNEL_PHY_OFFSET is not mapped.
// @param page_dir_phy_addr: The physical address of the page directory to
// initialize.
void paging_setup_new_page_dir(void * const page_dir_phy_addr);

// Recursively delete an address space. All physical frames mapped to user space
// will be freed, all user page tables and the page directory will be freed as
// well.
// @param addr_space: The address space to de-allocate.
// Note: This function cannot know if a particular physical frame mapped to user
// space (or a user page table) is used by another process (or even the kernel).
// Therefore it is not compatible with page table/frame sharing.
void paging_free_addr_space(struct addr_space * const addr_space);

// Debug function printing the contiguous ranges of virtual addresses that are
// mapped in the current address space.
void paging_walk(void);

// Execute tests related to paging.
void paging_test(void);
