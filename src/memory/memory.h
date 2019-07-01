// Memory manipulation functions.
#ifndef _MEMORY_H
#define _MEMORY_H
#include <includes/types.h>
// Copy a memory region to another.
// Copy `len` bytes.
void
memcpy(char *const to, char const *const from, size_t const len);

// Fill a memory region with `len` bytes to the value `byte`.
void
memset(char *const to, char const byte, size_t const len);
#endif
