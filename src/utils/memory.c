#include <utils/memory.h>

void
memcpy(uint8_t * const to, uint8_t const * const from, size_t const len) {
    for (size_t i = 0; i < len; ++i) {
        to[i] = from[i];
    }
}

void
memset(uint8_t * const to, uint8_t const byte, size_t const len) {
    for (size_t i = 0; i < len; ++i) {
        to[i] = byte;
    }
}

void
memzero(uint8_t * const to, size_t const len) {
    memset(to, 0, len);
}
