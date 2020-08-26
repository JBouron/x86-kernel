#pragma once
#include <lock.h>

typedef struct {
    spinlock_t readers_lock; 
    // Protected by readers_lock.
    uint32_t num_readers;
    spinlock_t writer_lock;
} rwlock_t;

#define INIT_RW_LOCK()                      \
    {                                       \
        .readers_lock = INIT_SPINLOCK(),    \
        .num_readers = 0,                   \
        .writer_lock = INIT_SPINLOCK(),     \
    }

#define DECLARE_RW_LOCK(name) \
    rwlock_t name = INIT_RW_LOCK()

void rwlock_init(rwlock_t * const lock);

void rwlock_read_lock(rwlock_t * const lock);
void rwlock_read_unlock(rwlock_t * const lock);

void rwlock_write_lock(rwlock_t * const lock);
void rwlock_write_unlock(rwlock_t * const lock);

void rwlock_test(void);
