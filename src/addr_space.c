#include <addr_space.h>
#include <debug.h>
#include <kmalloc.h>
#include <frame_alloc.h>
#include <paging.h>
#include <acpi.h>
#include <error.h>

// The kernel's address space needs to be statically allocated since it will be
// used even before dynamic allocation is setup.
struct addr_space KERNEL_ADDR_SPACE = {
    .lock = INIT_SPINLOCK(),
    .page_dir_phy_addr = NULL,
};

// Each cpu has the curr_addr_space variable which contains a pointer to the
// struct addr_space associated with the address space that it is currently
// using.
DECLARE_PER_CPU(struct addr_space *, curr_addr_space);

struct addr_space *get_curr_addr_space(void) {
    if (!cpu_paging_enabled()) {
        // We are very early in the boot process, return the physical address of
        // KERNEL_ADDR_SPACE.
        return to_phys(&KERNEL_ADDR_SPACE);
    } else if (percpu_initialized()) {
        return this_cpu_var(curr_addr_space);
    } else {
        // If percpu has not yet been enabled then we can only be in the kernel
        // address space.
        return &KERNEL_ADDR_SPACE;
    }
}

void switch_to_addr_space(struct addr_space * const addr_space) {
    ASSERT(addr_space->page_dir_phy_addr);

    // Disable interrupts while changing address space. This is because there is
    // no way to set cr3 and the curr_addr_space variable atomically.
    bool const irqs = interrupts_enabled();
    cpu_set_interrupt_flag(false);

    if (percpu_initialized()) {
        // switch_to_addr_space() is also called in init_paging(). At this point
        // percpu is not initialized and it is not safe to use it.
        this_cpu_var(curr_addr_space) = addr_space;
    }
    cpu_set_cr3(addr_space->page_dir_phy_addr);

    cpu_set_interrupt_flag(irqs);
}

struct addr_space *get_kernel_addr_space(void) {
    // This function can be used before and after paging is enabled. Hence be
    // careful and return the pointer in the correct address space.
    if (cpu_paging_enabled()) {
        return &KERNEL_ADDR_SPACE;
    } else {
        return to_phys(&KERNEL_ADDR_SPACE);
    }
}

void const * get_kernel_page_dir_phy_addr(void) {
    return get_kernel_addr_space()->page_dir_phy_addr;
}

void init_kernel_addr_space(void * const page_dir_phy_addr) {
    struct addr_space * const addr_space = get_kernel_addr_space();
    if (addr_space->page_dir_phy_addr) {
        PANIC("Kernel address space has already been initialized.");
    }
    addr_space->page_dir_phy_addr = page_dir_phy_addr;
}

void lock_addr_space(struct addr_space * const addr_space) {
    spinlock_lock(&addr_space->lock);
}

void unlock_addr_space(struct addr_space * const addr_space) {
    spinlock_unlock(&addr_space->lock);
}

struct addr_space *create_new_addr_space(void) {
    struct addr_space * clone = kmalloc(sizeof(*clone));
    if (!clone) {
        SET_ERROR("Cannot allocated struct addr_space", 0);
        return NULL;
    }

    void * const page_dir = alloc_frame();
    if (page_dir == NO_FRAME) {
        SET_ERROR("Cannot allocate page dir for new address space", 0);
        kfree(clone);
        return NULL;
    }

    spinlock_init(&clone->lock);
    clone->page_dir_phy_addr = page_dir;

    paging_setup_new_page_dir(clone->page_dir_phy_addr);

    return clone;
}

void delete_addr_space(struct addr_space * const addr_space) {
    if (addr_space == &KERNEL_ADDR_SPACE) {
        // Nothing good can come out of this.
        PANIC("Are you insane ?\n");
    }

    // Make sure that no cpu is currently using the address space. Note: For now
    // there is no guarantee that remote cpus are not trying to switch to this
    // address space. Care should be taken here FIXME.
    uint32_t const ncpus = acpi_get_number_cpus();
    for (uint32_t cpu = 0; cpu < ncpus; ++cpu) {
        struct addr_space const * const cpu_as = cpu_var(curr_addr_space, cpu);
        if (cpu_as == addr_space) {
            PANIC("Tried to delete an address space used by cpu %d\n", cpu);
        }
    }

    paging_free_addr_space(addr_space);

    // Address space must _always_ be created using the create_new_addr_space()
    // which dynamically allocate the struct addr_space. The only exception is
    // KERNEL_ADDR_SPACE but this one should _never_ be deleted.
    kfree(addr_space);
}

#include <addr_space.test>
