// Use freestanding libs to get fixed size types and useful macros.
#ifndef _UTILS_TYPES_H
#define _UTILS_TYPES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

// Some typdef making reading code easier.
// Physical address.
typedef uint32_t p_addr;
// Linear address.
typedef uint32_t l_addr;
// Virtual address.
typedef uint32_t v_addr;

#endif
