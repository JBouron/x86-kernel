// Useful functions and macros to debug the kernel.
#pragma once
// We are using the tty to print debug messages. The tty (as well as the
// underlying buffer which should be VGA) should, obviously, never fail.
#include <tty.h>
#include <asm.h>

// Macros coming from the linux kernel source code.
#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

// The logging function is no less that the tty_printf function.
#define LOG tty_printf

// Panic the kernel, output the error message an dump some states into the tty.
#define PANIC(...) \
    do { \
        LOG("----[ PANIC! ]----\n"); \
        LOG("Kernel panic at %s:%d\n", __FILE__, __LINE__); \
        LOG(__VA_ARGS__); \
        lock_up(); \
    } while(0)

#define ASSERT(condition) \
    do { \
        if(!(condition)) { \
            PANIC("Condition failed: " __stringify(condition) "\n"); \
        } \
    } while(0)

#define STATIC_ASSERT(condition, msg)    _Static_assert(condition, msg)

// Mark unreachable code.
#define __UNREACHABLE__ PANIC("UNREACHABLE")
