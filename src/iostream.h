#pragma once
#include <types.h>
// This file describes the basic iostream structure abstracting basic I/O stream
// devices (eg. no peek).

struct io_stream_t {
    // Read bytes from an input stream. Note that the read operation might be
    // blocking depending on the underlying implementation.
    // @param dest: The target buffer to read the stream into.
    // @param length: The length of the buffer to read into.
    // @return: The number of bytes successfully read from the input stream.
    size_t (*read)(uint8_t * const dest, size_t const length);

    // Write bytes into an output stream. Note that the write operation might be
    // blocking depending on the underlying implementation.
    // @param buf: The buffer to write into the output stream.
    // @param length: The number of bytes to write into the output stream.
    // @return: The number of bytes successfully written into the output stream.
    size_t (*write)(uint8_t const * const buf, size_t const length);
};

// Define some short-hands for input stream (not implementing write) and output
// streams (not implementing read).
#define i_stream_t  io_stream_t
#define o_stream_t  io_stream_t
