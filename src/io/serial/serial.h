#pragma once
#include <utils/types.h>
#include <devices/char_device.h>

// The serial device is a character device and as such shares the same
// interface.
struct serial_dev_t {
    // This is the common device abstraction and must be the first field.
    struct char_dev_t dev;
    // The port to be used.
    uint16_t port;
    // The baud rate used by this device.
    uint32_t baud_rate;
} __attribute__((packed));

// The port used by the serial console.
extern uint16_t const SERIAL_PORT;

// Initialise the serial device to the correct baud rate with the correct
// parameters (bits, parity, stop bit(s)).
void
serial_init_dev(struct serial_dev_t * const dev, uint16_t const port);

