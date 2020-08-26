#include <rw_lock.h>
#include <memory.h>
#include <debug.h>

void rwlock_init(rwlock_t * const lock) {
    rwlock_t const default_lock = INIT_RW_LOCK();
    memcpy(lock, &default_lock, sizeof(*lock));
}

void rwlock_read_lock(rwlock_t * const lock) {
    spinlock_lock(&lock->readers_lock);
    lock->num_readers ++;

    if (lock->num_readers == 1) {
        // This is the first reader, acquire the write lock to block writers.
        spinlock_lock(&lock->writer_lock);
    }
    spinlock_unlock(&lock->readers_lock);
}

void rwlock_read_unlock(rwlock_t * const lock) {
    spinlock_lock(&lock->readers_lock);
    lock->num_readers --;

    if (!lock->num_readers) {
        // This is the last reader, releas the write lock to unblock writers.
        // FIXME: Since the first and last readers might be different cpus, we
        // need to fixup the owner field in the spinlock. This is disgusting.
        // The interrupts_enabled is always false since the writer lock is
        // acquired after the reader lock and hence interrupts are disabled.
        lock->writer_lock.owner = this_cpu_var(cpu_id);
        ASSERT(!lock->writer_lock.interrupts_enabled);
        spinlock_unlock(&lock->writer_lock);
    }
    spinlock_unlock(&lock->readers_lock);
}


void rwlock_write_lock(rwlock_t * const lock) {
    spinlock_lock(&lock->writer_lock);
}

void rwlock_write_unlock(rwlock_t * const lock) {
    spinlock_unlock(&lock->writer_lock);
}

#include <rw_lock.test>
