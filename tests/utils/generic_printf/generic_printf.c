#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <utils/generic_printf/generic_printf.h>

#define BUFFER_SIZE 256
// This is the buffer that the print function will write to during the test.
static char BUFFER[BUFFER_SIZE];
// The current position in the buffer, used by the putc function as a pointer
// for the next char to put.
static size_t pos = 0;

// Put characters into the BUFFER defined above.
static void
fake_putc(char const c) {
    BUFFER[pos] = c;
    pos ++;
    if (pos == BUFFER_SIZE) {
        // The printf function most likely wrote too much data, fail.
        exit(EXIT_FAILURE);
    }
}

static void
g_printf(const char * const fmt, ...) {
    // We use the generic_printf function with the fake_putc function as backend
    va_list list;
    va_start(list, fmt);
    generic_printf(fake_putc, fmt, list);
    va_end(list);
}

// Test the generic printf function implementation.
int
main(int argc, char **argv) {
    uint32_t const u32_value = 0xDEAD;
    int32_t const i32_value_pos = 123;
    int32_t const i32_value_neg = -12;
    uint32_t const u32_value_big = 0xCAFEBABE;
    char const * const str = "HELLO world\n\t";
    char const c = '$';
    uint8_t const small = 7;
    uint8_t const * const ptr = (uint8_t *)0xFFFFD10A;


    char const * const fmt = "AC %u%d %dDE%, AO%xQWE, |%s|#@# char=%c\n%d--%p";

    char const * const expected = "AC 57005123 -12DE%, AO0xCAFEBABEQWE, |HELLO world\n\t|#@# char=$\n7--0xFFFFD10A";

    memset(BUFFER, 0, BUFFER_SIZE);
    g_printf(fmt,
             u32_value,
             i32_value_pos,
             i32_value_neg,
             u32_value_big,
             str,
             c,
             small,
             ptr);

    // Compare the strings.
    if (strlen(expected) != strlen(BUFFER)) {
        return 1;
    }
    for (size_t i = 0; i < strlen(expected); ++i) {
        if (expected[i]!=BUFFER[i]) {
            return 1;
        }
    }

    return EXIT_SUCCESS;
}
