// A basic tty manipulating text on top of the VGA text buffer provided by the
// BIOS.
// For now there can only be one tty at a time since there is only one VGA
// buffer. Thus there is not tty "object"/"struct" per se but rather global
// functions and static variables.
#ifndef _TTY_TTY_H
#define _TTY_TTY_H
#include <utils/types.h>
#include <vga/vga.h>

// Initialize the tty.
void
tty_init(void);

// Print a char `c` in the tty. Handles line wraps and newline characters.
void
tty_putc(const char c);

// Print a formatted string in the tty.
void
tty_printf(const char * const fmt, ...);

// Set the current foreground color of the tty. This color will be used for all
// subsequent characters written in the tty.
void
tty_set_fg_color(enum vga_color_t const color);

// Set the current background color of the tty. This color will be used for all
// subsequent characters written in the tty.
void
tty_set_bg_color(enum vga_color_t const color);
#endif
