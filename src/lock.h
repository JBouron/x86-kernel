#pragma once
#include <percpu.h>
#include <cpu.h>

// Spinlock state.
struct spinlock_t {
    // The underlying lock. If the lock is free, this field is 0, otherwise it
    // is 1.
    uint8_t lock;
    // The state of the interrupt flag of the cpu holding this lock _before_
    // acquiring the lock. This allows us to disable interrupts in critical
    // sections and restore them after releasing the lock (if they were enabled
    // before acquiring the lock).
    bool interrupts_enabled;
};

#define INIT_SPINLOCK() \
    {               \
        .lock = 0,  \
    }

#define DECLARE_SPINLOCK(name)  \
    struct spinlock_t name = INIT_SPINLOCK();

// INTERNAL. Do the actual lock operation on the spinlock. This function should
// never be used outside the implementation of spinlocks. Use spinlock_lock
// instead.
// @param lock: The spinlock to acquire.
void _spinlock_lock(struct spinlock_t * const lock);

// INTERNAL. Do the actual unlock operation on the spinlock. This function
// should never be used outside the implementation of spinlocks. Use
// spinlock_unlock instead.
// @param lock: The spinlock to release.
void _spinlock_unlock(struct spinlock_t * const lock);

// Acquire the lock on a spinlock. Returns only when the lock has been acquired
// by the current cpu.
// @param lock: The spinlock to acquire.
void spinlock_lock(struct spinlock_t * const lock);

// Release a spinlock.
// @param lock: The spinlock to release.
void spinlock_unlock(struct spinlock_t * const lock);
