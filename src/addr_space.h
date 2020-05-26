#pragma once

#include <types.h>
#include <lock.h>

// This struct represent the address space of a process or kernel.
struct addr_space {
    // Since a given address space can be used by multiple cores at a time, any
    // operation on the address space (i.e. on the page directory or page tables
    // of the addr space) needs to be protected by a lock.
    spinlock_t lock;

    // The physical address of the page directory describing this address space.
    void * page_dir_phy_addr;
};

// Each cpu has the curr_addr_space variable which contains a pointer to the
// struct addr_space associated with the address space that it is currently
// using.
DECLARE_PER_CPU(struct addr_space *, curr_addr_space);

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

// Lock the address space currently used on this core.
void lock_curr_addr_space(void);

// Unlock the address space currently used on this core.
void unlock_curr_addr_space(void);

// Create a new address space. The address space initially contains the kernel
// data (and mappings).
// @return: The struct addr_space associated with this new address space.
struct addr_space *create_new_addr_space(void);

// Execute tests related to address space.
void addr_space_test(void);
