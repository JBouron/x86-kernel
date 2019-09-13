// Memory manipulation functions.
#ifndef _UTILS_MEMORY_H
#define _UTILS_MEMORY_H
#include <utils/types.h>
// Copy a memory region to another.
// Copy `len` bytes.
void
memcpy(void * const to, void const * const from, size_t const len);

// Fill a memory region with `len` bytes to the value `byte`.
void
memset(void * const to, uint8_t const byte, size_t const len);

// Fill a memory region with `len` bytes to the value 0.
void
memzero(void * const to, size_t const len);
#endif
