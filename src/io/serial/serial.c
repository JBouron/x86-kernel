#include <io/serial/serial.h>
#include <asm/asm.h>
#include <generic_printf/generic_printf.h>
#include <utils/debug.h>

// We use COM1 as the serial port.
uint16_t const SERIAL_PORT = 0x3F8;
// The baud rate to be used by the serial console.
uint32_t const BAUD_RATE = 38400;

// Writer into the register `reg` in the serial port `SERIAL_PORT`.
static void
_write_serial_register(uint8_t const reg, uint8_t const data) {
    ASSERT(reg < 8);
    uint16_t const addr = SERIAL_PORT + reg;
    outb(addr, data);
}

// Read a special register of the serial port `SERIAL_PORT`.
static uint8_t
_read_serial_register(uint8_t const reg) {
    ASSERT(reg < 8);
    uint16_t const addr = SERIAL_PORT + reg;
    return inb(addr);
}

static void
_set_dlab(uint8_t const dlab) {
    // DLAB is the most significant bit of the line control register.
    ASSERT(dlab == 1 || dlab == 0);
    uint8_t const line_control_reg = 0x3;
    uint8_t const previous = _read_serial_register(line_control_reg);
    uint8_t new = 0;
    if(dlab) {
        new = 0x80 | previous;
    } else {
        new = (~0x80) & previous;
    }
    _write_serial_register(line_control_reg, new);
}

void
serial_init(void) {
    // For now the serial console does not use interrupts. Disable them.
    _write_serial_register(0x1, 0x00);

    // Set the divisor to match the baud rate. 115200 is the maximum baud rate
    // possible.
    ASSERT(115200%BAUD_RATE == 0);
    uint16_t const divisor = 115200/BAUD_RATE;
    // Write high and low bytes of the divisor.
    _set_dlab(1);
    _write_serial_register(0x0, divisor >> 8);
    _write_serial_register(0x1, 0x0F & divisor);
    _set_dlab(0);

    // For the serial console we use 8-bit chars, no parity and one stop bit.
    uint8_t const char_width = 0x3;    // 8-bits chars.
    uint8_t const parity = 0x0 << 2;     // No parity.
    uint8_t const stop_bit = 0x0 << 3;   // 1 stop bit.
    uint8_t const control_reg_value = char_width | parity | stop_bit;
    _write_serial_register(0x3, control_reg_value);
}

void
serial_putc(char const c) {
    if(c == '\n') {
        _write_serial_register(0x0, '\r');
    }
    _write_serial_register(0x0, c);
}

void
serial_printf(const char * const fmt, ...) {
    // We use the generic_printf function with the tty_putc function as backend.
    va_list list;
    va_start(list, fmt);
    generic_printf(serial_putc, fmt, list);
    va_end(list);
}

char
serial_readc(void) {
    char const c = _read_serial_register(0x0);
    return c;
}
