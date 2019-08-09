#include <utils/memory.h>

// Test the memset function.

int main(int const argc, char const ** const argv) {
    uint8_t buf[] = {
        16,
        12,
        1,
        11
    };

    // memset the buffer to a specific value.
    uint8_t const value = 117;
    memset(buf, value, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); ++i) {
        if (buf[i] != value) {
            // FAILURE.
            return 1;
        }
    }
    return 0;
}
