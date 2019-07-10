#include <string/string.h>

size_t
strlen(const char * const str) {
    size_t i;
    for (i = 0; str[i]; ++i) {}
    return i;
}

bool
streq(const char * const str1, const char * const str2) {
    size_t const len1 = strlen(str1);
    size_t const len2 = strlen(str2);
    if (len1 != len2) {
        return false;
    } else {
        for (size_t i = 0; i < len1; ++i) {
            if (str1[i] != str2[i]) {
                return false;
            }
        }
        return true;
    }
}
