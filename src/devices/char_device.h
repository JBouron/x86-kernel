#ifndef _DEVICES_CHAR_DEVICE_H
#define _DEVICES_CHAR_DEVICE_H

// Definition of a character device supporting read and write operations.
struct char_dev_t {
    // Read at most `len` bytes from the device into the `dest` buffer. Returns
    // the number of bytes read.
    size_t (*read)(struct char_dev_t *dev,
                   uint8_t * const dest,
                   size_t const len);
    // Write at most `len` byte into the device from `buf`. Return the number of
    // bytes written.
    size_t (*write)(struct char_dev_t *dev,
                    uint8_t const * const buf,
                    size_t const len);
} __attribute__((packed));
#endif
