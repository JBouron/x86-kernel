#pragma once
#include <types.h>

#define PAGE_SIZE   0x1000

// Initialize and enable paging. After calling this function the processor uses
// the higher-half kernel (EIP, stack and GDTR point to the higher-half).
// @param esp: The stack pointer value right before calling this function. This
// parameter is important as it allows us to fixup the return address pushed on
// the stack as a _physical_ address.
void init_paging(void const * const esp);

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
                uint32_t const flags);

// Unmap a virtual memory region.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
void paging_unmap(void const * const vaddr, size_t const len);

// Unmap a virtual memory region and free the physical frames it was mapped to.
// @param vaddr: The virtual address to unmap. Must be 4KiB aligned.
// @param len: The length of the memory area.
void paging_unmap_and_free_frames(void const * const vaddr, size_t const len);

// Find a memory region in the current virtual address space with at least a
// certain number of contiguous pages non mapped.
// @param start_addr: The start address to search from.
// @parma npages: The minimum number of contiguous non mapped pages to look for.
// @return: The start virtual address of the memory region.
void *paging_find_contiguous_non_mapped_pages(void * const start_addr,
                                              size_t const npages);

// Maps physical frames to virtual memory above a certain address. The mapping
// will be continous in the virtual address space. Notes: The frames will not
// necessarily mapped at `start_addr` exactly but this function guarantees it
// will be mapped to an address >= start_addr.
// @param start_addr: The min virtual address to map the frames to.
// @param frames: An array containing the physical frames to be mapped to
// virtual memory.
// @param npages: The number of physical frames to mapped.
// @param flags: The flags to use when mapping the physical frames.
// @return: The start virtual address of the memory region.
void *paging_map_frames_above(void * const start_addr,
                              void ** frames,
                              size_t const npages,
                              uint32_t const flags);

// Setup a new page directory. This function will initialized the page directory
// with the entries used by the kernel as well as the recursive entry. Any
// address below KERNEL_PHY_OFFSET is not mapped.
// @param page_dir_phy_addr: The physical address of the page directory to
// initialize.
void paging_setup_new_page_dir(void * const page_dir_phy_addr);

// Delete a page directory, all its page tables (excepted page tables used by
// the kernel). Physical frames used by this directory and any non-kernel page
// table will be freed.
// @param page_dir_phy_addr: The physical address of the page directory to be
// deleted.
// Note: Physical frames associated to the address space (i.e. mapped in the
// address space) will not be freed. This is because this function does not know
// if a particular physical frame is tied to this address space only or if it is
// shared with another address space.
void paging_delete_page_dir(void * const page_dir_phy_addr);

// Execute tests related to paging.
void paging_test(void);
