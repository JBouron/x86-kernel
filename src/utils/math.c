#include <utils/math.h>

uint32_t max(uint32_t const x, uint32_t const y) {
    return ((x) > (y)) ? (x) : (y);
}

uint32_t min(uint32_t const x, uint32_t const y) {
    return ((x) < (y)) ? (x) : (y);
}

uint32_t ceil_x_over_y(uint32_t const x, uint32_t const y) {
    return ((x) / (y)) + (((x) % (y)) != 0);
}
