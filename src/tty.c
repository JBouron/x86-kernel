#include <tty.h>
#include <string.h>
#include <memory.h>


// The input and output streams to be used by the tty.
static struct i_stream_t const * INPUT_STREAM;
static struct o_stream_t const * OUTPUT_STREAM;

static void putc(char const chr) {
    OUTPUT_STREAM->write((uint8_t*)&chr, 1);
}

// Output an unsigned integer into the output stream in base 10 format.
// @param n: The unsigned integer to output.
static void output_uint(uint64_t const n, bool const is_64_bits) {
    // `digits` is an array that will contain the char representation of all
    // digits of `n` in reverse order. For instance if n == 123 then digits will
    // be ['3', '2', '1', '\0', ..., '\0'].
    uint8_t const max_digits = is_64_bits ? 19 : 9;
    char digits[max_digits];
    uint8_t digit_idx = 0;
    // Loop over n, determine each base-10 digits and store them in `digits`.
    uint64_t curr = n;
    if (n == 0) {
        putc('0');
        return;
    }
    while (curr) {
        uint64_t rem = curr % 10;
        char const rem_char = '0' + rem;
        digits[digit_idx++] = rem_char;
        curr /= 10;
    }
    // Print each digit, reversing the order ...
    for (int8_t i = digit_idx-1; i >= 0; --i) {
        putc(digits[i]);
    }
}

// Output a signed integer into the output stream in base 10 format.
// @param n: The signed integer to output.
static void output_sint(int64_t const n, bool const is_64_bits) {
    if (n < 0) {
        putc('-');
        output_uint((uint64_t)(-n), is_64_bits);
    } else {
        output_uint((uint64_t)(n), is_64_bits);
    }
}

// Output an unsigned integer into the output stream in hexadecimal
// format.
// @param n: The unsigned integer to output.
static void output_uint64_t_hex(uint64_t const n, bool const is_64_bits) {
    putc('0');
    putc('x');
    for(int8_t i = is_64_bits ? 60 : 28; i >= 0; i -= 4) {
        uint64_t const mask = 0xF << i;
        uint8_t const half_byte = (n & mask) >> (i);
        if (half_byte < 0xA) {
            putc('0' + half_byte);
        } else {
            putc('A' + (half_byte-0xA));
        }
    }

}

// Handle a substitution in the formatted string.
// @param fmt: Pointer on the pointer pointing on the current char in the
// caller. This pointer gets updated to point to the first char after the
// substitution pattern in the format string.
// @param list: The value list containing the values to be plugged in the
// string.
static void handle_substitution(char const **fmt, va_list *list) {
    char const *curr = *fmt;
    // Read the next char to figure out the type we need to write.
    char const type = *(curr++);
    // We have the type, simply read the value from the va_list.
    switch (type) {
        case 'd': {
            int32_t const val = va_arg((*list), int32_t);
            output_sint(val, false);
            break;
        }
        case 'D': {
            int64_t const val = va_arg((*list), int64_t);
            output_sint(val, true);
            break;
        }
        case 'u': {
            uint32_t const val = va_arg((*list), uint32_t);
            output_uint(val, false);
            break;
        }
        case 'U': {
            uint64_t const val = va_arg((*list), uint64_t);
            output_uint(val, true);
            break;
        }
        case 'p': {
            // FALL-THROUGH
        }
        case 'x': {
            uint32_t const val = va_arg((*list), uint32_t);
            output_uint64_t_hex(val, false);
            break;
        }
        case 'X': {
            uint64_t const val = va_arg((*list), uint64_t);
            output_uint64_t_hex(val, true);
            break;
        }
        case 's': {
            char const * const str = va_arg((*list), char const * const);
            for(size_t i = 0; i < strlen(str); ++i) {
                putc(str[i]);
            }
            break;
        }
        case 'c': {
            // Values smaller than an int are promoted to an int in the variadic
            // argument list. Hence for char we use int.
            char const val = va_arg((*list), int);
            putc(val);
            break;
        }
        default: {
            // For any other param, print the % and the char and move along.
            putc('%');
            putc(type);
            break;
        }
    }
    // Skip the substitution char.
    *fmt = curr;
}

// Do the actual printf in the output stream.
// @param fmt: The formatted string.
// @param list: The list of values to be inserted in the string.
static void do_printf(const char * const fmt, va_list list) {
    char const *curr = fmt;
    while (curr) {
        // Skip all the character that does not mark a substitution.
        while (*curr && (*curr != '%')) {
            putc(*(curr++));
        }

        if (!*curr) {
            // We reach the end of the string, we can return now.
            return;
        }

        // Skip the '%'.
        curr++;

        if (!*curr) {
            // We reach the end of the string, we can return now.
            return;
        }

        // Handle the substitution.
        handle_substitution(&curr, &list);
    }
}

// Initialize the tty with both an input and output stream.
// @param input_stream: The input stream to be used by the tty.
// @param output_stream: The output stream to be used by the tty.
void tty_init(struct i_stream_t const * const input_stream,
              struct o_stream_t const * const output_stream) {
    INPUT_STREAM = input_stream;
    OUTPUT_STREAM = output_stream;
}

// Print a formatted string to the output stream.
// @param fmt: The format string.
// @param __VA_ARGS: The values to use in the formatted string.
void tty_printf(const char * const fmt, ...) {
    // We use the generic_printf function with the tty_putc function as backend.
    va_list list;
    va_start(list, fmt);
    do_printf(fmt, list);
    va_end(list);
}
