#include <string/string.h>

size_t
strlen(const char *const str) {
    size_t i;
    for (i = 0; str[i]; ++i) {}
    return i;
}
