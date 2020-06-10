// Use freestanding libs to get fixed size types and useful macros.
#pragma once
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
// CPU register.
typedef uint32_t reg_t;
// Process ID.
typedef uint32_t pid_t;
// Index of a sector of a disk.
typedef uint64_t sector_t;
typedef uint64_t off_t;
