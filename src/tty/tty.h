// A TTY is a wrapper around a char device implementing putc and printf, amoung
// other things.
// In this kernel, the TTY struct is a singleton.
#ifndef _TTY_TTY_H
#define _TTY_TTY_H
#include <utils/types.h>
#include <vga/vga.h>
#include <devices/char_device.h>

struct tty_t {
    struct char_dev_t * dev;
};

// The global tty.
extern struct tty_t TTY;

// Initialize the tty with a given char device.
void
tty_init(struct char_dev_t * const cdev);

// Print a char `c` in the tty. Handles line wraps and newline characters.
void
tty_putc(const char c);

// Print a formatted string in the tty.
void
tty_printf(const char * const fmt, ...);
#endif
