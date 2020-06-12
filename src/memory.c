#include <memory.h>

void memcpy(void * const to, void const * const from, size_t const len) {
    uint8_t * const __to = (uint8_t*)to;
    uint8_t const * const __from = (uint8_t*)from;
    for (size_t i = 0; i < len; ++i) {
        __to[i] = __from[i];
    }
}

void memset(void * const to, uint8_t const byte, size_t const len) {
    uint8_t * const __to = (uint8_t*)to;
    for (size_t i = 0; i < len; ++i) {
        __to[i] = byte;
    }
}

void memzero(void * const to, size_t const len) {
    memset(to, 0, len);
}

bool memeq(void const * const s1, void const * const s2, size_t const size) {
    uint8_t const * __s1 = (uint8_t const *)s1;
    uint8_t const * __s2 = (uint8_t const *)s2;
    for (size_t i = 0; i < size; ++i) {
        if (*__s1 != *__s2) {
            return false;
        }
        __s1 ++;
        __s2 ++;
    }
    return true;
}

// Testing.
#include <memory.test>
