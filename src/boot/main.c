#include <utils/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <utils/string.h>
#include <asm/asm.h>
#include <memory/gdt.h>
#include <interrupt/interrupt.h>
#include <interrupt/handlers.h>
#include <utils/debug.h>
#include <asm/cpuid/cpuid.h>
#include <io/serial/serial.h>
#include <interrupt/apic/apic.h>
#include <boot/multiboot.h>
#include <boot/cmdline/cmdline.h>
#include <utils/kernel_map.h>
#include <memory/paging/alloc/alloc.h>
#include <memory/paging/paging.h>

// This is the global serial device used to log in the kernel.
static struct serial_dev_t SERIAL_DEVICE;

// Check all the assumptions we are making in this kernel. At least the ones
// that are checkable at boot time.
void
assumptions_check(void) {
    // We are using the CPUID extensively, as such we only support processor
    // that have this feature. This is done by checking if we can flip the bit
    // 21 in EFLAGS.
    uint32_t const eflags = read_eflags();
    uint32_t const bit21 = (eflags & (1 << 21)) >> 21;
    uint32_t const eflags_no_21 = eflags & (~(1<<21));
    uint32_t const new_eflags = eflags_no_21 | (bit21 ? 0 : 1<<21);
    write_eflags(new_eflags);
    ASSERT(read_eflags() & (1<<21));

    // For now (or maybe forever), we are supporting Intel processors only. Read
    // the vendorId to make sure.
    char eax[5], ebx[5], ecx[5], edx[5];
    cpuid(0x0, 0x0,
          (uint32_t*)eax, (uint32_t*)ebx, (uint32_t*)ecx, (uint32_t*)edx);
    eax[4] = 0;
    ebx[4] = 0;
    ecx[4] = 0;
    edx[4] = 0;
    ASSERT(streq(ebx, "Genu"));
    ASSERT(streq(edx, "ineI"));
    ASSERT(streq(ecx, "ntel"));

    // We require the APIC.
    ASSERT(is_apic_present());
}

void
kernel_main(struct multiboot_info_t const * const multiboot_info) {
    ASSERT(multiboot_info);
    // TODO: command line parsing is disabled until page fault is handled
    // correctly or a mechanism to read from physical memory is implemented.
    //char const * const cmdline = (char const * const)(multiboot_info->cmdline);
    //cmdline_parse(cmdline);

    // If the kernel has been started with the --wait flag then loop until gdb
    // attaches to the virtual machine and resume the execution.
    while(CMDLINE_PARAMS.wait_start) {
        asm("pause");
    }
    paging_init();

    // Disable the interrupts and the PIC. 
    interrupts_disable();
    pic_disable();

    // Initialize the serial console and the tty.
    uint16_t const serial_port = 0x3F8;
    serial_init_dev(&SERIAL_DEVICE, serial_port);
    tty_init(((struct char_dev_t *)&SERIAL_DEVICE));

    // We now initialize the GDT and IDT. We do that after the serial and tty
    // initialization since we want logs. In practice it doesn't matter that
    // much since we are already in a flat segment.
    gdt_init();
    idt_init();

    // Log some memory info.
    LOG("KERNEL_START         = %x\n", KERNEL_START);
    LOG("SECTION_TEXT_START   = %x\n", SECTION_TEXT_START);
    LOG("SECTION_TEXT_END     = %x\n", SECTION_TEXT_END);
    LOG("SECTION_RODATA_START = %x\n", SECTION_RODATA_START);
    LOG("SECTION_RODATA_END   = %x\n", SECTION_RODATA_END);
    LOG("SECTION_DATA_START   = %x\n", SECTION_DATA_START);
    LOG("SECTION_DATA_END     = %x\n", SECTION_DATA_END);
    LOG("SECTION_BSS_START    = %x\n", SECTION_BSS_START);
    LOG("SECTION_BSS_END      = %x\n", SECTION_BSS_END);
    LOG("KERNEL_END           = %x\n", KERNEL_END);
    LOG("KERNEL_SIZE          = %d bytes.\n", KERNEL_SIZE);
    LOG("SECTION_TEXT_SIZE    = %d bytes.\n", SECTION_TEXT_SIZE);
    LOG("SECTION_RODATA_SIZE  = %d bytes.\n", SECTION_RODATA_SIZE);
    LOG("SECTION_DATA_SIZE    = %d bytes.\n", SECTION_DATA_SIZE);
    LOG("SECTION_BSS_SIZE     = %d bytes.\n", SECTION_BSS_SIZE);

    // Check that all the assumptions made during the development of this kernel
    // hold on the current machine. If not, it will panic.
    assumptions_check();

    // Add an example interrupt handler for interrupt 33.
    struct interrupt_gate_t handler33 = {
        .vector = 33,
        .offset = interrupt_handler_33,
        .segment_selector = 0x8,
    };
    idt_register_handler(&handler33);

    // Initialize and enable the APIC.
    apic_init();
    apic_enable();
    interrupts_enable();

    //apic_start_periodic_timer(33);

    paging_dump_pagedir();
    // Lock up the computer.
    lock_up();
}
