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
#include <serial.h>
#include <segmentation.h>
#include <interrupt.h>
#include <lapic.h>
#include <bitmap.h>
#include <frame_alloc.h>
#include <paging.h>

// Execute all the tests in the kernel.
static void test_kernel(void) {
    LOG("Running tests:\n");
    vga_test();
    mem_test();
    str_test();
    math_test();
    tty_test();
    cpu_test();
    serial_test();
    segmentation_test();
    interrupt_test();
    lapic_test();
    bitmap_test();
    frame_alloc_test();
    paging_test();
    print_test_summary();
}

// Trigger a shutdown of the virtual machine. This function has an undefined
// behaviour for bare-metal installations.
static void virt_shutdown(void) {
    // With QEMU writing 0x2000 into the port 0x604 triggers the shutdown.
    cpu_outw(0x604, 0x2000);
}

void
kernel_main(struct multiboot_info const * const multiboot_info) {
    ASSERT(multiboot_info);
    vga_init();
    tty_init(NULL, &SERIAL_STREAM);
    interrupt_init();
    cpu_set_interrupt_flag(true);
    init_lapic();
    test_kernel();
    virt_shutdown();
}
