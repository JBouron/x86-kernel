#pragma once
#include <types.h>
#include <debug.h>
#include <lapic.h>

// A testing function is simply a function retuning a boolean indicating
// success/failure.
typedef bool (*test_function)(void);

// Run a single test function. This function is not meant to be used directly,
// one should use the TEST_FWK_RUN macro instead.
// @param func: A pointer on the test function.
// @param file: Name of the testing function. This is used to pretty print the
// testing results.
void __run_single_test(test_function const func, char const * const name);

// Short-hand for __run_single_test generating the name from the function name.
#define TEST_FWK_RUN(func) \
    __run_single_test(func, #func)

// Print a summary of all the tests executed so far.
void print_test_summary(void);

#define SUCCESS do { return true; } while (0);
#define FAILURE do { return false; } while (0);

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            LOG(#cond); \
            LOG("\n"); \
            BREAK(); \
            FAILURE \
        }\
    } while(0)

#define TEST_WAIT_FOR(cond, timeout)    \
    do {                                \
        uint32_t remaining = timeout;   \
        while (!(cond)) {               \
            if (!remaining && timeout) {\
                BREAK();                \
                FAILURE                 \
            }                           \
            lapic_sleep(1);             \
            remaining --;               \
        }                               \
    } while (0)
