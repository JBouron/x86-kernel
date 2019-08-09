#include <utils/memory.h>

// Test the memzero function.

int main(int const argc, char const ** const argv) {
    uint8_t buf[] = {
        16,
        12,
        1,
        11
    };

    // memzero the buffer.
    memzero(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); ++i) {
        if (buf[i]) {
            // FAILURE.
            return 1;
        }
    }
    return 0;
}
