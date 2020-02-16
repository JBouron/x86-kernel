#pragma once

// Spinlock state.
struct spinlock_t {
    // The underlying lock. If the lock is free, this field is 0, otherwise it
    // is 1.
    uint8_t lock;
};

#define DECLARE_SPINLOCK(name)  \
    struct spinlock_t name = {  \
        .lock = 0,              \
    };

// Acquire the lock on a spinlock. Returns only when the lock has been acquired
// by the current cpu.
// @param lock: The spinlock to acquire.
void spinlock_lock(struct spinlock_t * const lock);

// Release a spinlock.
// @param lock: The spinlock to release.
void spinlock_unlock(struct spinlock_t * const lock);
