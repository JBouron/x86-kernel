#ifndef _UTILS_MATH_H
#define _UTILS_MATH_H

// Computes ceil(x/y).
#define ceil_x_over_y(x, y) (((x) / (y)) + (((x) % (y)) != 0))

// Compute min/max(x, y).
#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

#endif
