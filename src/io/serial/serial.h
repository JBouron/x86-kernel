#ifndef _SERIAL_H
#define _SERIAL_H
#include <utils/types.h>

// The port used by the serial console.
extern uint16_t const SERIAL_PORT;

// Initialise the serial port to the correct baud rate with the correct
// parameters (bits, parity, stop bit(s)).
void
serial_init(void);

// Write a char to the serial port.
void
serial_putc(char const c);

// Read a char from the serial port.
char
serial_readc(void);

// Print a formatted string into the serial port.
void
serial_printf(const char * const fmt, ...);
#endif
