#include <types.h>
#include <vga.h>
#include <tty.h>
#include <string.h>
#include <cpu.h>
#include <debug.h>
#include <cpuid.h>
#include <multiboot.h>
#include <memory.h>
#include <math.h>
#include <test.h>

// Execute all the tests in the kernel.
static void test_kernel(void) {
    LOG("Running tests:\n");
    vga_test();
    mem_test();
    str_test();
    math_test();
    tty_test();
    cpu_test();
    print_test_summary();
}

void
kernel_main(struct multiboot_info const * const multiboot_info) {
    ASSERT(multiboot_info);
    //int i = 0;
    //while(!i);
    uint64_t const start = read_tsc();
    vga_init();
    tty_init(NULL, &VGA_STREAM);

    test_kernel();

    uint64_t const end = read_tsc();
    uint64_t const delta = end - start;

    LOG("Start = %D\n",start);
    LOG("End   = %D\n",end);
    LOG("Init took %u cycles.\n", delta);

    uint64_t const tsc_msr = read_msr(0x10);
    uint64_t const tsc_ins = read_tsc();
    uint64_t const tsc_ins2 = read_tsc();
    LOG("TSC from MSR = %U\n", tsc_msr);
    LOG("TSC from INS = %U\n", tsc_ins);
    LOG("TSC from INS2= %U\n", tsc_ins2);

    LOG("CPU has CPUID support: %d\n", has_cpuid());
    lock_up();
}
