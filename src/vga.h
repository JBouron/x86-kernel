// Functions, types and constants used to interact with the VGA text matrix.
#pragma once
#include <types.h>
#include <iostream.h>

// The VGA matrix is abstracted by an io_stream where only the write operation
// is available.
extern struct io_stream_t VGA_STREAM;

// Initialize the VGA text matrix.
void vga_init(void);

// Set the buffer address to be used as the VGA matrix.
// @param addr: The new address to be used for the VGA matrix.
void vga_set_buffer_addr(uint16_t * const addr);

// Execute tests on the VGA functions.
void vga_test(void);
