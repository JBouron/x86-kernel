#include <spinlock.h>
#include <cpu.h>
#include <debug.h>
#include <memory.h>

// This file contains the definitions of spinlock_lock and spinlock_unlock.

// Get the Id of the current cpu.
// @return: The id of the current cpu.
static uint8_t get_curr_cpu_id(void) {
    // Since locks are used very early in the boot sequence, percpu data might
    // not be yet setup. In this case fall back to cpu_apic_id() which uses the
    // cpuid instruction to get the cpu id.
    if (percpu_initialized()) {
        return this_cpu_var(cpu_id);
    } else {
        return cpu_apic_id();
    }
}

// Do the actual lock operation on the spinlock. Definition in the assembly
// file.
// @param lock: The spinlock to acquire.
// @param irq_enabled: If true, the cpu will enable interrupt while waiting for
// the lock to be available.
void _spinlock_lock(spinlock_t * const lock, bool const irq_enabled);

// Do the actual unlock operation on the spinlock. Definition in the assembly
// file.
// @param lock: The spinlock to release.
void _spinlock_unlock(spinlock_t * const lock);

void spinlock_init(spinlock_t * const lock) {
    spinlock_t const default_lock = INIT_SPINLOCK();
    memcpy(lock, &default_lock, sizeof(default_lock));
}

void spinlock_lock(spinlock_t * const lock) {
    bool const irq = interrupts_enabled();

    // If interrupts are enabled while calling the spinlock_lock() then enable
    // interrupt while waiting for the lock.
    _spinlock_lock(lock, irq);

    // Interrupts are expected to be disabled when acquiring the lock.
    ASSERT(!interrupts_enabled());

    // We can now mutate the fields of the lock.
    lock->interrupts_enabled = irq;

    // The owner field is reset upon unlocking.
    ASSERT(lock->owner == 0xFF);
    lock->owner = get_curr_cpu_id();
}

void spinlock_unlock(spinlock_t * const lock) {
    // Get back the saved IF from the lock.
    bool const interrupts = lock->interrupts_enabled;

    ASSERT(lock->owner == get_curr_cpu_id());
    lock->owner = 0xFF;

    _spinlock_unlock(lock);
    // Restore the IF to its previous value before the spinlock_lock.
    cpu_set_interrupt_flag(interrupts);
}

bool spinlock_is_held(spinlock_t const * const lock) {
    return lock->lock == 1 && lock->owner == get_curr_cpu_id();
}

#include <spinlock_def.test>
