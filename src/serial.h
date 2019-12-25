#pragma once
#include <iostream.h>

// This is the implementation of the serial I/O interface.

// The serial I/O stream. This is the exported interface of the serial logic.
extern struct io_stream_t SERIAL_STREAM;

// Initialize the serial port to be ready for communication with the outside
// world.
void serial_init(void);

// Execute all tests related to the serial controller logic.
void serial_test(void);
