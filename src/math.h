#pragma once

#include <types.h>

// Compute ceil(x/y).
uint32_t ceil_x_over_y_u32(uint32_t const x, uint32_t const y);

// Compute min/max(x, y).
uint32_t max_u32(uint32_t const x, uint32_t const y);
uint32_t min_u32(uint32_t const x, uint32_t const y);

// Test the functions defined above.
void math_test(void);
