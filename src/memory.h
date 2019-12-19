// Memory manipulation functions.
#pragma once
#include <types.h>

// Copy a memory region to another _in the same memory address space_.
// @param to: The destination of the copy.
// @param fomr: The source of the copy.
// @param len: The number of bytes to copy from `from` to `to`.
// Note: The copy is performed on a byte basis.
void memcpy(void * const to, void const * const from, size_t const len);

// Fill a memory region with a certain value.
// @param to: The memory region to fill.
// @param byte: The value to write in each byte of the memory region.
// @param len: The number of bytes in the memory region.
void memset(void * const to, uint8_t const byte, size_t const len);

// Fill a memory region with NULL bytes. This is a short-hand for memset(to, 0,
// len).
// @param to: The memory region to fill.
// @param len: The number of bytes in the memory region.
void memzero(void * const to, size_t const len);

// Execute tests of the above functions.
void mem_test(void);
