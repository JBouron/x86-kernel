#pragma once
#include <types.h>
#include <vga.h>

// A tty is a unifying interface handling a input stream (keyboard, serial
// connection, ...) and an output stream (vga, serial, ...).

// Initialize the tty with both an input and output stream.
// @param input_stream: The input stream to be used by the tty.
// @param output_stream: The output stream to be used by the tty.
void tty_init(struct i_stream_t const * const input_stream,
              struct o_stream_t const * const output_stream);

// Print a formatted string to the output stream.
// @param fmt: The format string.
// @param __VA_ARGS: The values to use in the formatted string.
void tty_printf(const char * const fmt, ...);

// Testing of the tty.
void tty_test(void);
