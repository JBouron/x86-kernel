#include <generic_printf/generic_printf.h>

void _gen_print_uint32_t(generic_putc_t putc, uint32_t const n) {
    // `digits` is an array that will contain the char representation of all
    // digits of `n` in reverse order. For instance if n == 123 then digits will
    // be ['3','2','1','\0',...,'\0'].
    uint8_t const max_digits = 9;
    char digits[max_digits];
    uint8_t digit_idx = 0;
    // Loop over n, determine each base-10 digits and store them in `digits`.
    uint32_t curr = n;
    while (curr) {
        uint32_t rem = curr % 10;
        char const rem_char = '0' + rem;
        digits[digit_idx++] = rem_char;
        curr /= 10;
    }
    // Print each digit, reversing the order ...
    for (int8_t i = digit_idx-1; i >= 0; --i) {
        putc(digits[i]);
    }
}

void _gen_print_int32_t(generic_putc_t putc, int32_t const n) {
    if (n < 0) {
        putc('-');
        _gen_print_uint32_t(putc,(uint32_t)(-n));
    } else {
        _gen_print_uint32_t(putc,(uint32_t)(n));
    }
}

void generic_printf(generic_putc_t putc, const char *const fmt, va_list list) {
    char const *curr = fmt;
    while (curr) {
        // Skip all the character that does not mark a substitution.
        while (*curr&&(*curr!='%')) {
            putc(*(curr++));
        }
        // Skip the '%'.
        curr++;

        if (!*curr) {
            // We reach the end of the string, we can return now.
            return;
        }

        // Read the next char to figure out the type we need to write.
        char const type = *(curr++);
        // We have the type, simply read the value from the va_list.
        switch (type) {
            case 'd': {
                int32_t const val = va_arg(list,int32_t);
                _gen_print_int32_t(putc,val);
                break;
            }
            case 'u': {
                uint32_t const val = va_arg(list,uint32_t);
                _gen_print_uint32_t(putc,val);
                break;
            }
            default: {
                // For any other param, print the char and move along.
                putc(type);
                break;
            }
        }
    }
}


