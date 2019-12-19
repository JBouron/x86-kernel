#include <math.h>

uint32_t max_u32(uint32_t const x, uint32_t const y) {
    return ((x) > (y)) ? (x) : (y);
}

uint32_t min_u32(uint32_t const x, uint32_t const y) {
    return ((x) < (y)) ? (x) : (y);
}

uint32_t ceil_x_over_y_u32(uint32_t const x, uint32_t const y) {
    return ((x) / (y)) + (((x) % (y)) != 0);
}

#include <math.test>
