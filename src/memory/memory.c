#include <memory/memory.h>

void
memcpy(char *const to, char const *const from, size_t const len) {
    for (size_t i = 0; i < len; ++i) {
        to[i] = from[i];
    }
}

void
memset(char *const to, char const byte, size_t const len) {
    for (size_t i = 0; i < len; ++i) {
        to[i] = byte;
    }
}
