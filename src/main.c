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
#include <kernel_map.h>
#include <multiboot.h>
#include <list.h>
#include <kmalloc.h>
#include <acpi.h>
#include <ioapic.h>
#include <smp.h>
#include <percpu.h>
#include <ipm.h>
#include <atomic.h>
#include <addr_space.h>
#include <proc.h>
#include <sched.h>

// Execute all the tests in the kernel.
void test_kernel(void) {
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
    multiboot_test();
    list_test();
    kmalloc_test();
    ioapic_test();
    smp_test();
    percpu_test();
    ipm_test();
    atomic_test();
    addr_space_test();
    proc_test();

    print_test_summary();
}

// Resign from the BSP role, that is clear the BSP bit in to the
// IA32_APIC_BASE_MSR. The reason is that this bit inhibit any INIT IPI on the
// BSP which makes it impossible to execute init_aps test from APs.
static void resign_bsp(void) {
    uint32_t const IA32_APIC_BASE_MSR = 0x1B;
    uint64_t const msr = read_msr(IA32_APIC_BASE_MSR);
    // Bit 8 indicates if the cpu is BSP or not. It is R/W.
    write_msr(IA32_APIC_BASE_MSR, msr & (~(1 << 8)));
}

// Initialize global kernel state.
static void init_kernel_state(void) {
    // Start by initializing logging as early as possible to be able to log
    // anywhere.
    vga_init();
    serial_init();
    tty_init(NULL, &SERIAL_STREAM);

    // Parse ACPI tables.
    acpi_init();

    // Parsing the ACPI tables gave us the number of cpus on the system. We can
    // now proceed to allocate percpu areas.
    init_percpu();
    // Switch to the final GDT containing percpu segments.
    switch_to_final_gdt(PER_CPU_OFFSETS);

    // Setup the BSP's TSS.
    extern uint8_t stack_top;
    this_cpu_var(kernel_stack) = &stack_top;
    setup_tss();
}

void kernel_main(struct multiboot_info const * const multiboot_info) {
    ASSERT(multiboot_info);

    // Initialize global kernel state.
    init_kernel_state();

    // Initialize interrupts, LAPIC and IOAPIC.
    interrupt_init();
    init_lapic();
    init_ioapic();

    // Calibrate the lapic timer frequency.
    calibrate_timer();

    // Disable the BSP bit on the current cpu. The reason is that this bit
    // inhibit any INIT IPI on the BSP which makes it impossible to execute
    // init_aps test from APs.
    resign_bsp();

    // Initialize IPM mechanism before waking the APs up so that they can use
    // it ASAP.
    init_ipm();

    // Wake up Application Processors.
    init_aps();

    // Run tests.
    test_kernel();

    sched_init();
    sched_start();
    __UNREACHABLE__;
}
