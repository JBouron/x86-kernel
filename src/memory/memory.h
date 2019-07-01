// Memory manipulation functions.
#ifndef _MEMORY_H
#define _MEMORY_H
#include <includes/types.h>
// Copy a memory region to another.
// Copy `len` bytes.
void
memcpy(uint8_t *const to, uint8_t const *const from, size_t const len);

// Fill a memory region with `len` bytes to the value `byte`.
void
memset(uint8_t *const to, uint8_t const byte, size_t const len);

// Fill a memory region with `len` bytes to the value 0.
void
memzero(uint8_t *const to, size_t const len);
#endif
