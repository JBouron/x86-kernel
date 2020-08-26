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

// Get the ACPI index of the target cpu for the current cpu.
// When testing, a the current cpu might want to use other cpu(s) to simulate a
// scenario. A good rule of thumb is to use <cpu> + 1, <cpu> + 2, ... where
// <cpu> the ACPI cpu index of the current cpu. However, since the index of the
// current cpu might not necessarily be 0, we need to % this number by ncpus.
// This function makes this whole computation simpler in the following way:
//  - Any cpu on the system as a set of target remote cpus that it can use for a
// given test. Each target cpu is given an index in [0, 1, ..., NCPUS - 2]. This
// is NOT the ACPI cpu index of the target cpu, just a new name for those.
//  - This function maps the index of a target cpu to its ACPI cpu index,
//  handles the % NCPUS and makes sure that the target is not the current cpu.
// Example:
//  - For current cpu = 0, TEST_TARGET_CPU(0) == 1, TEST_TARGET_CPU(2) == 2, ...
//  - For current cpu = 1, TEST_TARGET_CPU(0) == 2, TEST_TARGET_CPU(2) == 3, ...
// @param target_idx: The index of the target cpu.
// @return: The ACPI index of the target cpu.
uint8_t TEST_TARGET_CPU(uint8_t const target_idx);
