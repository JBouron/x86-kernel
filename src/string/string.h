// Basic string manipulation.
#ifndef _STRING_H
#define _STRING_H
#include <includes/types.h>

// Return the lenght of the string `str`.
// Strings are as per C specification and end with a '\0' character.
size_t
strlen(const char * const str);

// Compare two strings, return true if they are equal, that is same lenght and
// same content.
bool
streq(const char * const str1, const char * const str2);
#endif
