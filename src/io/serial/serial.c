#include <io/serial/serial.h>
#include <asm/asm.h>
#include <utils/generic_printf/generic_printf.h>
#include <utils/debug.h>

// We use this specific value for the baud rate for all serial devices.
#define DEFAULT_BAUD_RATE   (38400)

// A helper to convert a char_dev_t to a serial_dev_t.
#define SERIAL_DEV(d)   ((struct serial_dev_t *)(d))

// Writer into the register `reg` in the serial device.
static void
__write_serial_register(struct serial_dev_t * const dev,
                        uint8_t const reg,
                        uint8_t const data) {
    ASSERT(reg < 8);
    uint16_t const addr = dev->port + reg;
    outb(addr, data);
}

// Read a special register of the serial device.
static uint8_t
__read_serial_register(struct serial_dev_t * const dev, uint8_t const reg) {
    ASSERT(reg < 8);
    uint16_t const addr = dev->port + reg;
    return inb(addr);
}

// Write the dlab bit for a certain serial device given the port.
static void
__set_dlab(struct serial_dev_t * const dev, uint8_t const dlab) {
    // DLAB is the most significant bit of the line control register.
    ASSERT(dlab == 1 || dlab == 0);
    uint8_t const line_control_reg = 0x3;
    uint8_t const previous = __read_serial_register(dev, line_control_reg);
    uint8_t new = 0;
    if(dlab) {
        new = 0x80 | previous;
    } else {
        new = (~0x80) & previous;
    }
    __write_serial_register(dev, line_control_reg, new);
}

// This implement a blocking read from the serial device.
static size_t
__serial_read(struct char_dev_t * d, uint8_t * const dest, size_t const len) {
    struct serial_dev_t * const dev = SERIAL_DEV(d);

    for (size_t i = 0; i < len; ++i) {
        uint8_t const c = __read_serial_register(dev, 0x0);
        dest[i] = c;
    }
    return len;
}

// Write a buffer to the serial device.
static size_t
__serial_write(struct char_dev_t * d,
               uint8_t const * const buf,
               size_t const len) {
    struct serial_dev_t * const dev = SERIAL_DEV(d);

    for (size_t i = 0; i < len; ++i) {
        uint8_t const c = buf[i];
        if(c == '\n') {
            __write_serial_register(dev, 0x0, '\r');
        }
        __write_serial_register(dev, 0x0, c);
    }
    return len;
}

void
serial_init_dev(struct serial_dev_t * const dev, uint16_t const port) {
    uint32_t const baud_rate = DEFAULT_BAUD_RATE;
    // Set the private state of the device.
    dev->baud_rate = baud_rate;
    dev->port = port;
    // Set the function pointers for read() and write(), as required by the
    // char_dev_t abstraction.
    dev->dev.read = __serial_read;
    dev->dev.write = __serial_write;

    // For now the serial console does not use interrupts. Disable them.
    __write_serial_register(dev, 0x1, 0x00);

    // Set the divisor to match the baud rate. 115200 is the maximum baud rate
    // possible.
    ASSERT(115200%baud_rate == 0);
    uint16_t const divisor = 115200/baud_rate;
    // Write high and low bytes of the divisor.
    __set_dlab(dev, 1);
    __write_serial_register(dev, 0x0, divisor >> 8);
    __write_serial_register(dev, 0x1, 0x0F & divisor);
    __set_dlab(dev, 0);

    // For the serial console we use 8-bit chars, no parity and one stop bit.
    uint8_t const char_width = 0x3;    // 8-bits chars.
    uint8_t const parity = 0x0 << 2;     // No parity.
    uint8_t const stop_bit = 0x0 << 3;   // 1 stop bit.
    uint8_t const control_reg_value = char_width | parity | stop_bit;
    __write_serial_register(dev, 0x3, control_reg_value);
}
