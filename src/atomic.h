#pragma once
#include <types.h>

// This file provides functions for atomic primitives.

// An atomic_t is a structure containing a simple int32. This underlying value
// should _never_ be accessed directly.
typedef struct {
    int32_t value;
} atomic_t;

// Initialize an atomic_t to a specific value.
// @param atomic: Pointer to the atomic_t to initialize.
// @param value: Value to initialize the atomic_t to.
void atomic_init(atomic_t * const atomic, int32_t const value);

// Read the value of an atomic_t.
// @param atomic: Pointer to the atomic_t to read from.
// @return: The current value of `atomic`.
int32_t atomic_read(atomic_t * const atomic);

// Set the value of an atomic_t.
// @param atomic: A pointer to the target atomic_t.
// @param value: The value to write into the atomic_t.
void atomic_write(atomic_t * const atomic, int32_t const value);

// Increment an atomic_t.
// @param atomic: The atomic_t to increment.
void atomic_inc(atomic_t * const atomic);

// Decrement an atomic_t.
// @param atomic: The atomic_t to decrement.
void atomic_dec(atomic_t * const atomic);

// Add a value to the current value of an atomic_t.
// @param atomic: The atomic_t to update.
// @param value: The value to add to the atomic_t.
void atomic_add(atomic_t * const atomic, int32_t const value);

// Subtract a value from the current value of an atomic_t.
// @param atomic: The atomic_t to update.
// @param value: The value to add to the atomic_t.
void atomic_sub(atomic_t * const atomic, int32_t const value);

// Atomically read the value of an atomic_t and add a value to it.
// @param atomic: The target atomic_t.
// @param value: The value to add to the atomic_t after reading from it.
// @return: The value of the atomic_t _before_ adding to it.
int32_t atomic_fetch_and_add(atomic_t * const atomic, int32_t const value);

// Atomically read the value of an atomic_t and subtract a value from it.
// @param atomic: The target atomic_t.
// @param value: The value to add to the atomic_t after reading from it.
// @return: The value of the atomic_t _before_ subtracting from it.
int32_t atomic_fetch_and_sub(atomic_t * const atomic, int32_t const value);

// Atomically decrement an atomic_t and test it.
// @param atomic: The targeted atomic_t.
// @return: true if atomic became 0 after the decrement, false otherwise.
bool atomic_dec_and_test(atomic_t * const atomic);

// Execute tests related to atomic_ts.
void atomic_test(void);
