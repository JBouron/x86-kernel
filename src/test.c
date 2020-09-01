#include <test.h>
#include <debug.h>
#include <frame_alloc.h>
#include <kmalloc.h>
#include <lapic.h>
#include <acpi.h>

// Some test statistics:
// The number of tests run so far.
static uint32_t TESTS_COUNT = 0;
// The number of successful tests so far.
static uint32_t SUCCESS_COUNT = 0;

// The number of physical frames leaked during the tests.
static uint32_t TOT_PHY_FRAME_LEAK = 0;
// The size in bytes of dynamically allocated memory leaks in the tests.
static uint32_t TOT_DYN_MEM_LEAK = 0;

// Detect memory leaks (physical or dynamic) and prints a warning if any is
// found. This function makes sure not to output false positives due to timing
// (ex: a remote processor was about to free an IPM message).
// @param name: The name of the test.
// @param frames_before: The number of physical frames that were allocated
// before running the test.
// @param kmalloc_before: The number of bytes dynamically allocated before
// running the test.
static void detect_memory_leaks(char const * const name,
                                uint32_t const frames_before,
                                uint32_t const kmalloc_before) {
    uint32_t const max_tries = 10;
    uint32_t num_tries = 0;
    while (num_tries < max_tries &&
        (frames_allocated() != frames_before ||
        kmalloc_total_allocated() != kmalloc_before)) {
        num_tries ++;
        lapic_sleep(100);
    }

    uint32_t const allocated_frames_after = frames_allocated();
    size_t const kmalloc_tot_after = kmalloc_total_allocated();
    if (frames_before != allocated_frames_after) {
        uint32_t const num = allocated_frames_after - frames_before;
        WARN("  Physical frame leak of %u frames detected for %s\n", num, name);
        TOT_PHY_FRAME_LEAK += num;
    }
    if (kmalloc_before != kmalloc_tot_after) {
        uint32_t const num = kmalloc_tot_after - kmalloc_before;
        WARN("  Dynamic memory leak of %u bytes detected for %s\n", num, name);
        TOT_DYN_MEM_LEAK += num;
        kmalloc_list_allocations();
    }
}

void __run_single_test(test_function const func, char const * const name) {
    TESTS_COUNT ++;

    uint32_t const allocated_frames_before = frames_allocated();
    size_t const kmalloc_tot_before = kmalloc_total_allocated();

    bool const res = func();

    SUCCESS_COUNT += res ? 1 : 0;
    char const * const str = res?"\033[32m OK \033[39m":"\033[31mFAIL\033[39m";
    LOG("[%s] %s\n", str, name);
    detect_memory_leaks(name, allocated_frames_before, kmalloc_tot_before);
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

uint8_t TEST_TARGET_CPU(uint8_t const target_idx) {
    uint8_t const ncpus = acpi_get_number_cpus();
    ASSERT(target_idx + 2 <= ncpus);
    uint8_t const target = (this_cpu_var(cpu_id) + target_idx + 1) % ncpus;
    // This should not be necessary due to the first ASSERT.
    ASSERT(target != this_cpu_var(cpu_id));
    return target;
}
