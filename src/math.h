#pragma once

#include <types.h>

// Compute ceil(x/y).
uint32_t ceil_x_over_y_u32(uint32_t const x, uint32_t const y);

uint32_t round_up_u32(uint32_t const x, uint32_t const m);
uint32_t round_down_u32(uint32_t const x, uint32_t const m);

// Compute min/max(x, y).
uint32_t max_u32(uint32_t const x, uint32_t const y);
uint32_t min_u32(uint32_t const x, uint32_t const y);
uint64_t max_u64(uint64_t const x, uint64_t const y);
uint64_t min_u64(uint64_t const x, uint64_t const y);

// Test the functions defined above.
void math_test(void);
