#pragma once
#include <types.h>

// Implementation of a bitmap.

// This structure holds all of the state for a bitmap.
struct bitmap_t {
    // The size (in number of bits) that the bitmap can hold.
    uint32_t const size;
    // The number of free / available bits in the bitmap.
    uint32_t free;
    // The uint32_t array storing the actual bitmap.
    uint32_t * const data;
};

// Computes the number of uint32_t words necessary to hold a bitmap.
// @param size: The size (in bits) of the bitmap.
// Note: The size does not necessarily need to be a multiple of 32 bits.
#define BITMAP_WORD_COUNT(size) (((size) / (32)) + (((size) % (32)) != 0))

// Declare a static bitmap.
// @param _name: The _name of the bitmap variable to declare.
// @param _size: The size (in bits) of the bitmap to declare.
#define DECLARE_BITMAP(_name,_size)                                 \
    uint32_t _bitmap_ ## _name ## _data[BITMAP_WORD_COUNT(_size)];  \
    struct bitmap_t _name = {                                       \
        .size = _size,                                              \
        .free = _size,                                              \
        .data = _bitmap_ ## _name ## _data,                         \
    };

// Reset a bitmap. That is unset all the bits, making the bitmap empty.
// @param bitmap: The bitmap to reset.
void bitmap_reset(struct bitmap_t * const bitmap);

// Set a bit in the bitmap.
// @param bitmap: The target bitmap.
// @param index: The index of the bit to set.
void bitmap_set(struct bitmap_t * const bitmap, uint32_t const index);

// Get the value of a bit in the bitmap.
// @param bitmap: The target bitmap.
// @param index: The index of the bit to be read.
// @return: true if the bit is set in the bitmap, false otherwise.
bool bitmap_get_bit(struct bitmap_t const * const bitmap, uint32_t const index);

// Unset a bit in a bitmap.
// @param bitmap: The target bitmap.
// @parma index: The index of the bit to unset in the bitmap.
void bitmap_unset(struct bitmap_t * const bitmap, uint32_t const index);

#define BM_NPOS (~((uint32_t)0))
// Set the next available bit in a bitmap and return its index.
// @param bitmap: The target bitmap.
// @return: If a bit was available, returns its index in the bitmap, otherwise,
// returns BM_NPOS.
uint32_t bitmap_set_next_bit(struct bitmap_t * const bitmap);

// Check if a bitmap is full.
// @param bitmap: The target bitmap.
// @return: true if the bitmap is full, that is all the bits are set, false
// otherwise.
bool bitmap_is_full(struct bitmap_t const * const bitmap);

// Set all the bits in a bitmap.
// @param bitmap: The target bitmap.
void bitmap_set_all(struct bitmap_t * const bitmap);

// Execute tests of the bitmap functions.
void bitmap_test(void);
