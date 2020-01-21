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
    // The lapic test is disabled for now as it generate spurious interrupts
    // that cause panics.
    //lapic_test();
    bitmap_test();
    frame_alloc_test();
    paging_test();
    multiboot_test();
    print_test_summary();
}

static void pretty_print_mutliboot_hdr(struct multiboot_info const * hdr) {
    LOG("Memory low = %x\n", hdr->mem_lower * 1024);
    LOG("Memory hi  = %x\n", hdr->mem_upper * 1024);
    LOG("Mmap len  = %u\n", hdr->mmap_length);
    LOG("Mmap addr = %x\n", hdr->mmap_addr);

    void const * const addr = to_virt((void*)hdr->mmap_addr);
    struct multiboot_mmap_entry const * entry = (struct multiboot_mmap_entry*)addr;
    while (entry < (struct multiboot_mmap_entry*)(addr + hdr->mmap_length)) {
        uint64_t const start = entry->base_addr;
        uint64_t const end = start + entry->length - 1;
        char const * const type = entry->type == 1 ? "Available" : "Reserved ";
        LOG("%X - %X => %s (%X)\n", start, end, type, entry->length);
        entry ++;
    }
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

    LOG("\n");
    LOG("=== ### ===\n");
    LOG("Kernel loaded at %p-%p\n", to_phys(KERNEL_START_ADDR), to_phys(KERNEL_END_ADDR));
    LOG("Kernel size: %x bytes\n", KERNEL_SIZE);

    interrupt_init();
    cpu_set_interrupt_flag(true);
    init_lapic();
    test_kernel();

    LOG("Multiboot header at %p\n", multiboot_info);
    struct multiboot_info const * const hdr = to_virt(multiboot_info);
    pretty_print_mutliboot_hdr(hdr);
    virt_shutdown();
}
