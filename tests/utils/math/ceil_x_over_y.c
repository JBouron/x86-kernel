// Some test for the min/max functions provided under the utils/ directory.
// This is trivial.

#include <utils/math.h>
#include <stdint.h>
#include <stdio.h>

#define SUCCESS do { return 0; } while (0);
#define FAILURE do { return 1; } while (0);

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("%d\n",__LINE__); \
            FAILURE \
        }\
    } while(0)

int main(int argc, char **argv) {
    uint32_t const a = 3;
    uint32_t const b = 8;

    TEST_ASSERT(ceil_x_over_y( a, b ) == 1);
    TEST_ASSERT(ceil_x_over_y( b, a ) == 3);
    TEST_ASSERT(ceil_x_over_y( a, a ) == 1);
    TEST_ASSERT(ceil_x_over_y( 0, a ) == 0);

    SUCCESS;
}
