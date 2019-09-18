// A TTY is a wrapper around a char device implementing putc and printf, amoung
// other things.
// In this kernel, the TTY struct is a singleton.
#pragma once
#include <utils/types.h>
#include <vga/vga.h>
#include <devices/char_device.h>

struct tty {
    // The underlying device used by the tty to read from and write to.
    struct char_dev * dev;
};

// The global tty.
extern struct tty TTY;

// Initialize the tty with a given char device.
void
tty_init(struct char_dev * const cdev);

// Print a char `c` in the tty. Handles line wraps and newline characters.
void
tty_putc(const char c);

// Print a formatted string in the tty.
void
tty_printf(const char * const fmt, ...);
