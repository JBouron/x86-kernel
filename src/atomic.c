#include <atomic.h>
#include <types.h>

void atomic_init(atomic_t * const atomic, int32_t const value) {
    atomic->value = value;
}

int32_t atomic_read(atomic_t * const atomic) {
    // In x86, a read from an aligned word is atomic.
    return atomic->value;
}

void atomic_write(atomic_t * const atomic, int32_t const value) {
    // In x86, a write to an aligned word is atomic.
    atomic->value = value;
}

void atomic_inc(atomic_t * const atomic) {
    atomic_add(atomic, 1);
}

void atomic_dec(atomic_t * const atomic) {
    atomic_add(atomic, -1);
}

void atomic_sub(atomic_t * const atomic, int32_t const value) {
    atomic_add(atomic, -value);
}

int32_t atomic_fetch_and_sub(atomic_t * const atomic, int32_t const value) {
    return atomic_fetch_and_add(atomic, -value);
}

#include <atomic.test>
