#include <test.h>
#include <debug.h>
#include <frame_alloc.h>
#include <kmalloc.h>

// Some test statistics:
// The number of tests run so far.
static uint32_t TESTS_COUNT = 0;
// The number of successful tests so far.
static uint32_t SUCCESS_COUNT = 0;

// The number of physical frames leaked during the tests.
static uint32_t TOT_PHY_FRAME_LEAK = 0;
// The size in bytes of dynamically allocated memory leaks in the tests.
static uint32_t TOT_DYN_MEM_LEAK = 0;

void __run_single_test(test_function const func, char const * const name) {
    TESTS_COUNT ++;

    uint32_t const allocated_frames_before = frames_allocated();
    size_t const kmalloc_tot_before = kmalloc_total_allocated();

    bool const res = func();

    uint32_t const allocated_frames_after = frames_allocated();
    size_t const kmalloc_tot_after = kmalloc_total_allocated();

    SUCCESS_COUNT += res ? 1 : 0;
    char const * const str = res ? " OK " : "FAIL";
    LOG("[%s] %s\n", str, name);
    if (allocated_frames_before != allocated_frames_after) {
        uint32_t const num = allocated_frames_after - allocated_frames_before;
        WARN("  Physical frame leak of %u frames detected for %s\n", num, name);
        TOT_PHY_FRAME_LEAK += num;
    }
    if (kmalloc_tot_before != kmalloc_tot_after) {
        uint32_t const num = kmalloc_tot_after - kmalloc_tot_before;
        WARN("  Dynamic memory leak of %u bytes detected for %s\n", num, name);
        TOT_DYN_MEM_LEAK += num;
    }
}

void print_test_summary(void) {
    ASSERT(TESTS_COUNT >= SUCCESS_COUNT);
    LOG("=== Tests summary ===\n");
    if (TESTS_COUNT == SUCCESS_COUNT) {
        LOG("\033[32mAll %u tests passed\033[39m\n", TESTS_COUNT);
    } else {
        LOG("%u / %u tests failed\n", TESTS_COUNT - SUCCESS_COUNT, TESTS_COUNT);
    }
    if (TOT_PHY_FRAME_LEAK) {
        WARN("%u physical frame(s) leaked.\n", TOT_PHY_FRAME_LEAK);
    }
    if (TOT_DYN_MEM_LEAK) {
        WARN("%u dynamically allocated byte(s) leaked.\n", TOT_DYN_MEM_LEAK);
    }
    LOG("=====================\n");
}
