#include <utils/string.h>

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
        return strneq(str1, str2, len1);
    }
}

bool
strneq(const char * const str1, const char * const str2, size_t const n) {
    for (size_t i = 0; i < n; ++i) {
        if (str1[i] != str2[i]) {
            return false;
        }
    }
    return true;
}

size_t
str_find_chr(char const * const str, char const ch, size_t const begin) {
    if (begin >= strlen(str)) {
        // If the begin index is bigger than the size of the actual string we
        // return STR_NPOS.
        return STR_NPOS;
    }

    char const * curr = str + begin;
    while (*curr && *curr != ch) {
        curr ++;
    }

    if (!*curr) {
        // We have reached the end of the string while looking for ch, return
        // NPOS.
        return STR_NPOS;
    } else {
        size_t const len = curr - str;
        return len;
    }
}
