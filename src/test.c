#include <test.h>
#include <debug.h>

// Some test statistics:
// The number of tests run so far.
static uint32_t TESTS_COUNT = 0;
// The number of successful tests so far.
static uint32_t SUCCESS_COUNT = 0;

void __run_single_test(test_function const func, char const * const name) {
    TESTS_COUNT ++;
    bool const res = func();
    SUCCESS_COUNT += res ? 1 : 0;
    char const * const str = res ? " OK " : "FAIL";
    LOG("[%s] %s\n", str, name);
}

void print_test_summary(void) {
    ASSERT(TESTS_COUNT >= SUCCESS_COUNT);
    LOG("=== Tests summary ===\n");
    if (TESTS_COUNT == SUCCESS_COUNT) {
        LOG("All %u tests passed\n", TESTS_COUNT);
    } else {
        LOG("%u / %u tests failed\n", TESTS_COUNT - SUCCESS_COUNT, TESTS_COUNT);
    }
    LOG("=====================\n");
}
