// Basic string manipulation.
#pragma once
#include <types.h>

// Compute the length of a string.
// @param str: The NULL terminated string.
// @retunr: The number of bytes/character in `str`.
size_t strlen(const char * const str);

// Compare two strings.
// @param str1: First string operand.
// @param str2: Second string operand.
// @return: true if `str1` and `str2` have identical content, false otherwise.
bool streq(const char * const str1, const char * const str2);

// Compare two strings looking at their first N characters.
// @param str1: First string operand.
// @param str2: Second string operand.
// @param n: The number of characters to look at to decide on the result of the
// comparison.
// @return: true if the two strings have the same `n` first characters, false
// otherwise.
bool strneq(const char * const str1, const char * const str2, size_t const n);

// Find a character in a string from index `begin`. If no such character is
// found, return STR_NPOS.
// Find a character in a string.
// @param str: The string to look up the character into.
// @param ch: The character to look for.
// @param begin: The index in `str` at which the search operation should look
// like.
// @return: If `ch` is in `str` then return the first position/index of `ch`
// that is >= `begin`. Else return STR_NPOS (see below).
size_t str_find_chr(char const * const str, char const ch, size_t const begin);

void strncpy(char const * const src, char * const dst, size_t const len);

// STR_NPOS is used as a special return value of str_find_chr. It
// indicates that a character is not present in the string.
#define STR_NPOS    ((size_t) -1)

// Execute all available tests on the string operations.
void str_test(void);
