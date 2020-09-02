#pragma once

#include <types.h>
#include <spinlock.h>

// This struct represent the address space of a process or kernel.
struct addr_space {
    // Since a given address space can be used by multiple cores at a time, any
    // operation on the address space (i.e. on the page directory or page tables
    // of the addr space) needs to be protected by a lock.
    spinlock_t lock;

    // The physical address of the page directory describing this address space.
    void * page_dir_phy_addr;
};

// Get a pointer on the address space currently used by the cpu.
// @return: A pointer on the struct addr_space associated with the address space
// used by the cpu.
// Note: This function can be used in the following scenarios:
//  - Before paging is enabled, in which case the function returns the physical
//  address of KERNEL_ADDR_SPACE.
//  - After paging is enabled but before percpu is initialized, in this case the
//  function returns the virtual address of KERNEL_ADDR_SPACE.
//  - After paging is enabled and percpu initialized, in this case the function
//  will return the struct addr_space of the address space of the cpu.
struct addr_space *get_curr_addr_space(void);

// Switch to a new address space on the current cpu.
// @param addr_space: The address space to switch to.
// Note: This function can be called before percpu is initialized.
void switch_to_addr_space(struct addr_space * const addr_space);

// Get the struct addr_space associated with the kernel address space.
// @return: A pointer on the struct addr_space of the kernel's address space.
struct addr_space *get_kernel_addr_space(void);

// Get the physical address of the kernel's page directory.
// @return: The physical address of the kernel's page directory.
void const * get_kernel_page_dir_phy_addr(void);

// Initialize the kernel address space.
// @param page_dir_phy_addr: The physical address of the page directory to use
// for the kernel.
// Note: This function can only be called once.
void init_kernel_addr_space(void * const page_dir_phy_addr);

// Lock an address space.
// @param addr_space: The address space to be locked.
void lock_addr_space(struct addr_space * const addr_space);

// Unlock an address space.
// @param addr_space: The address space to be unlocked.
void unlock_addr_space(struct addr_space * const addr_space);

// Create a new address space. The address space initially contains the kernel
// data (and mappings).
// @return: On success, the struct addr_space associated with this new address
// space, otherwise NULL is returned.
struct addr_space *create_new_addr_space(void);

// Delete an address space and de-allocate the physical frames it uses for its
// page directory and page tables.
// @param addr_space: The address space to be deleted.
void delete_addr_space(struct addr_space * const addr_space);

// Execute tests related to address space.
void addr_space_test(void);
