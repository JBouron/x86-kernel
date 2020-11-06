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
        //
        // FIXME: We need to enable interrupts here as we are acquiring a second
        // lock and having interrupts disabled here could cause a deadlock if a
        // cpu with the writer lock tries to do a TLB shootdown.
        // Enabling interrupts here could be dangerous but for now we don't have
        // a choice.
        cpu_set_interrupt_flag(true);
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
        lock->writer_lock.owner = cpu_id();
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
