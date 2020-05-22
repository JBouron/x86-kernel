#include <addr_space.h>
#include <debug.h>

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

void lock_curr_addr_space(void) {
    struct addr_space * addr_space = NULL;
    if (!percpu_initialized()) {
        // We cannot access the curr_addr_space variable of the cpu, that means
        // we are still in the boot sequence, at that point there is only one
        // address space available : the kernel's.
        addr_space = get_kernel_addr_space();

        // Make sure that the cpu is using the kernel's address space.
        uint32_t const curr_cr3 = cpu_read_cr3();
        void * const curr_page_dir = (void*)(curr_cr3 & 0xFFFFF000);
        ASSERT(addr_space->page_dir_phy_addr == curr_page_dir);

    } else {
        addr_space = this_cpu_var(curr_addr_space);
    }
    
    spinlock_lock(&addr_space->lock);
}

void unlock_curr_addr_space(void) {
    struct addr_space * addr_space = NULL;
    if (!percpu_initialized()) {
        // We cannot access the curr_addr_space variable of the cpu, that means
        // we are still in the boot sequence, at that point there is only one
        // address space available : the kernel's.
        addr_space = get_kernel_addr_space();

        // Make sure that the cpu is using the kernel's address space.
        uint32_t const curr_cr3 = cpu_read_cr3();
        void * const curr_page_dir = (void*)(curr_cr3 & 0xFFFFF000);
        ASSERT(addr_space->page_dir_phy_addr == curr_page_dir);

    } else {
        addr_space = this_cpu_var(curr_addr_space);
    }
    
    spinlock_unlock(&addr_space->lock);
}

#include <addr_space.test>
