#include <types.h>
#include <vga.h>
#include <tty.h>
#include <string.h>
#include <cpu.h>
#include <debug.h>
#include <cpuid.h>
#include <multiboot.h>

// Check all the assumptions we are making in this kernel. At least the ones
// that are checkable at boot time.
void
assumptions_check(void) {
    // For now (or maybe forever), we are supporting Intel processors only. Read
    // the vendorId to make sure.
    char eax[5], ebx[5], ecx[5], edx[5];
    cpuid(0x0,
          (uint32_t*)eax, (uint32_t*)ebx, (uint32_t*)ecx, (uint32_t*)edx);
    eax[4] = 0;
    ebx[4] = 0;
    ecx[4] = 0;
    edx[4] = 0;
    ASSERT(streq(ebx, "Genu"));
    ASSERT(streq(edx, "ineI"));
    ASSERT(streq(ecx, "ntel"));
    LOG("%s%s%s\n", ebx, edx, ecx);
}

// Execute all the tests in the kernel.
static void test_kernel(void) {
    LOG("Running tests:\n");
    vga_test();
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
    LOG("TSC from MSR = %U\n", tsc_msr);
    LOG("TSC from INS = %U\n", tsc_ins);

    LOG("CPU has CPUID support: %d\n", has_cpuid());
    assumptions_check();
    lock_up();
}
