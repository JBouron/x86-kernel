#pragma once
#include <percpu.h>
#include <cpu.h>

// Spinlock state.
typedef struct {
    // The underlying lock. If the lock is free, this field is 0, otherwise it
    // is 1.
    uint8_t lock;
    // The state of the interrupt flag of the cpu holding this lock _before_
    // acquiring the lock. This allows us to disable interrupts in critical
    // sections and restore them after releasing the lock (if they were enabled
    // before acquiring the lock).
    bool interrupts_enabled;
    // The owner of the spinlock. That is the ID of the cpu currently holding
    // the lock. If the lock is not held this field is 0xFF.
    uint8_t owner;
} spinlock_t;

#define INIT_SPINLOCK() \
    {                   \
        .lock = 0,      \
        .owner = 0xFF,  \
    }

#define DECLARE_SPINLOCK(name)  \
    spinlock_t name = INIT_SPINLOCK();

// Initialize a spinlock to its default state (unlocked).
// @param lock: The spinlock_t to initialize.
void spinlock_init(spinlock_t * const lock);

// Acquire the lock on a spinlock. Returns only when the lock has been acquired
// by the current cpu.
// @param lock: The spinlock to acquire.
void spinlock_lock(spinlock_t * const lock);

// Release a spinlock.
// @param lock: The spinlock to release.
void spinlock_unlock(spinlock_t * const lock);

// Check if a given spinlock is held by the current cpu.
bool spinlock_is_held(spinlock_t const * const lock);

// Run spinlock tests.
void spinlock_test(void);
