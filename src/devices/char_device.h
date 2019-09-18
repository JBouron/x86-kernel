#pragma once

// Definition of a character device supporting read and write operations.
struct char_dev {
    // Read at most `len` bytes from the device into the `dest` buffer. Returns
    // the number of bytes read.
    size_t (*read)(struct char_dev *dev,
                   uint8_t * const dest,
                   size_t const len);
    // Write at most `len` byte into the device from `buf`. Return the number of
    // bytes written.
    size_t (*write)(struct char_dev *dev,
                    uint8_t const * const buf,
                    size_t const len);
} __attribute__((packed));
