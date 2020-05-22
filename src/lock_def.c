#include <lock.h>
#include <cpu.h>
#include <debug.h>

// This file contains the definitions of spinlock_lock and spinlock_unlock.

void spinlock_lock(struct spinlock * const lock) {
    // We don't have a way to atomically acquire a lock and disable interrupts,
    // therefore disable interrupts even before trying to acquire the lock.
    bool const irq = interrupts_enabled();
    cpu_set_interrupt_flag(false);

    _spinlock_lock(lock);

    // We can now mutate the fields of the lock.
    lock->interrupts_enabled = irq;
}

void spinlock_unlock(struct spinlock * const lock) {
    // Get back the saved IF from the lock.
    bool const interrupts = lock->interrupts_enabled;

    _spinlock_unlock(lock);
    // Restore the IF to its previous value before the spinlock_lock.
    cpu_set_interrupt_flag(interrupts);
}
