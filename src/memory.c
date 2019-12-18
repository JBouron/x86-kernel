#include <memory.h>

void
memcpy(void * const to, void const * const from, size_t const len) {
    uint8_t * const __to = (uint8_t*)to;
    uint8_t const * const __from = (uint8_t*)from;
    for (size_t i = 0; i < len; ++i) {
        __to[i] = __from[i];
    }
}

void
memset(void * const to, uint8_t const byte, size_t const len) {
    uint8_t * const __to = (uint8_t*)to;
    for (size_t i = 0; i < len; ++i) {
        __to[i] = byte;
    }
}

void
memzero(void * const to, size_t const len) {
    memset(to, 0, len);
}
