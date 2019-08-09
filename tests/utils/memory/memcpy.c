#include <utils/memory.h>

int main(int const argc, char const ** const argv) {
    uint8_t buf[] = {
        16,
        12,
        1,
        11
    };

    uint8_t new_buf[sizeof(buf)];

    // Copy buf in new buf and compare them.
    memcpy(new_buf, buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); ++i) {
        if (new_buf[i] != buf[i]) {
            // FAILURE.
            return 1;
        }
    }
    return 0;
}
