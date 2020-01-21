#include <math.h>

uint32_t max_u32(uint32_t const x, uint32_t const y) {
    return ((x) > (y)) ? (x) : (y);
}

uint32_t min_u32(uint32_t const x, uint32_t const y) {
    return ((x) < (y)) ? (x) : (y);
}

uint64_t max_u64(uint64_t const x, uint64_t const y) {
    return ((x) > (y)) ? (x) : (y);
}

uint64_t min_u64(uint64_t const x, uint64_t const y) {
    return ((x) < (y)) ? (x) : (y);
}

uint32_t ceil_x_over_y_u32(uint32_t const x, uint32_t const y) {
    return ((x) / (y)) + (((x) % (y)) != 0);
}

uint32_t round_up_u32(uint32_t const x, uint32_t const m) {
    return ((x / m) + (x % m != 0)) * m;
}

uint32_t round_down_u32(uint32_t const x, uint32_t const m) {
    return (x / m) * m;
}

#include <math.test>
