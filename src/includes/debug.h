// Useful functions and macros to debug the kernel.
#ifndef _DEBUG_H
#define _DEBUG_H
// We are using the tty to print debug message.
#include <tty/tty.h>

// The logging function is no less that the tty_printf function.
#define LOG tty_printf
#endif
