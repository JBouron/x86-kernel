// Basic string manipulation.
#pragma once
#include <utils/types.h>

// Return the lenght of the string `str`.
// Strings are as per C specification and end with a '\0' character.
size_t
strlen(const char * const str);

// Compare two strings, return true if they are equal, that is same lenght and
// same content.
bool
streq(const char * const str1, const char * const str2);

// Compare two strings up to n character. Note that there are no check on the
// length of the strings and so n can be bigger that the lenght of the strings.
bool
strneq(const char * const str1, const char * const str2, size_t const n);

#define STR_NPOS    ((size_t) -1)
// Find a character in a string from index `begin`. If no such character is
// found, return STR_NPOS.
size_t
str_find_chr(char const * const str, char const ch, size_t const begin);
