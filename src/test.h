#pragma once
#include <types.h>

// A testing function is simply a function retuning a boolean indicating
// success/failure.
typedef bool (*test_function)(void);

// Run a single test function.
// @param func: A pointer on the test function.
// @param file: Name of the testing function. This is used to pretty print the
// testing results.
void __run_single_test(test_function const func, char const * const name);

// Short-hand for __run_single_test generating the name from the function name.
#define TEST_FWK_RUN(func) \
    __run_single_test(func, #func)

#define SUCCESS do { return true; } while (0);
#define FAILURE do { return false; } while (0);

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            FAILURE \
        }\
    } while(0)
