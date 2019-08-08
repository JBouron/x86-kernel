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
    uint32_t const a = 33;
    uint32_t const b = 24;

    // Test min.
    TEST_ASSERT(min( a, a ) == a);
    TEST_ASSERT(min( a, b ) == b);
    TEST_ASSERT(min( b, b ) == b);
    TEST_ASSERT(min( b, a ) == b);

    // Test max.
    TEST_ASSERT(max( a, a ) == a);
    TEST_ASSERT(max( a, b ) == a);
    TEST_ASSERT(max( b, b ) == b);
    TEST_ASSERT(max( b, a ) == a);

    SUCCESS;
}
