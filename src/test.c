#include <test.h>
#include <debug.h>
#include <frame_alloc.h>

// Some test statistics:
// The number of tests run so far.
static uint32_t TESTS_COUNT = 0;
// The number of successful tests so far.
static uint32_t SUCCESS_COUNT = 0;

void __run_single_test(test_function const func, char const * const name) {
    TESTS_COUNT ++;
    uint32_t const allocated_frames_before = frames_allocated();
    bool const res = func();
    uint32_t const allocated_frames_after = frames_allocated();
    SUCCESS_COUNT += res ? 1 : 0;
    char const * const str = res ? " OK " : "FAIL";
    LOG("[%s] %s\n", str, name);
    if (allocated_frames_before != allocated_frames_after) {
        uint32_t const num = allocated_frames_after - allocated_frames_before;
        LOG("   Physical frame leak of %u frames detected for %s\n", num, name);
    }
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
