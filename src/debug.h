// Useful functions and macros to debug the kernel.
#pragma once
// We are using the tty to print debug messages. The tty (as well as the
// underlying buffer which should be VGA) should, obviously, never fail.
#include <tty.h>
#include <cpu.h>
#include <percpu.h>

// Macros coming from the linux kernel source code.
#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

// The logging function is no less that the tty_printf function.
#define LOG(...) tty_printf(__VA_ARGS__)
#define WARN(format, ...) tty_printf("\033[33m" format "\033[39m", __VA_ARGS__)
#define ERR(format, ...) tty_printf("\033[31m" format "\033[39m", __VA_ARGS__)

// Panic the kernel, output the error message an dump some states into the tty.
#define PANIC(...) \
    do { \
        LOG("----[ CPU %u PANIC! ]----\n", cpu_apic_id()); \
        LOG("Kernel panic at %s:%d\n", __FILE__, __LINE__); \
        LOG(__VA_ARGS__); \
        lock_up(); \
    } while(0)

// Temporary macro to indicate that an error condition should be propagated to
// the caller of (this) function. For now this will PANIC if cond is true.
#define TODO_PROPAGATE_ERROR(condition)     \
    do {                                    \
        if ((condition)) {                  \
            PANIC(__stringify(condition));  \
        }                                   \
    } while(0)

inline void PANIC_ASM(void) {
    PANIC("Panic in asm file.");
}

#define ASSERT(condition) \
    do { \
        if(!(condition)) { \
            PANIC("Condition failed: " __stringify(condition) "\n"); \
        } \
    } while(0)

#define STATIC_ASSERT(condition, msg)    _Static_assert(condition, msg)

// Mark unreachable code.
#define __UNREACHABLE__ PANIC("UNREACHABLE")

// Break in the code until the debugger manually sets i to != 0.
#define BREAK()              \
    do {                     \
        volatile int i = 0;  \
        while(!i);           \
    } while (0);

// Conditional break allowing to call BREAK() only if the cpu allows it (that is
// when the cond_break variable is true).
DECLARE_PER_CPU(bool, cond_break) = false;
#define CBREAK()                                                \
    do {                                                        \
        if (this_cpu_var(cond_break)) {                         \
            BREAK();                                            \
        }                                                       \
    } while (0)

