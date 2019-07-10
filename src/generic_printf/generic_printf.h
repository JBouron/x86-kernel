// Defines generic printf functions. Those function take an implementation of
// putc as argument and use this function to print every character.
#ifndef _GENERIC_PRINTF
#define _GENERIC_PRINTF
#include <string/string.h>

// The generic functions provided by this header expect a putc function that can
// output characters.
typedef void (*generic_putc_t)(char const);

// Prints a formatted string using the provided generic putc function.
void
generic_printf(generic_putc_t putc, const char * const fmt, va_list list);
#endif
