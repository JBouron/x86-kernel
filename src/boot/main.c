#include <utils/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <utils/string.h>
#include <asm/asm.h>
#include <memory/gdt/gdt.h>
#include <interrupt/interrupt.h>
#include <utils/debug.h>
#include <asm/cpuid/cpuid.h>
#include <io/serial/serial.h>
#include <interrupt/apic/apic.h>
#include <boot/multiboot.h>
#include <boot/cmdline/cmdline.h>
#include <utils/kernel_map.h>
#include <memory/paging/alloc/alloc.h>
#include <memory/paging/paging.h>
#include <interrupt/apic/ioapic.h>

// This is the global serial device used to log in the kernel.
static struct serial_dev_t SERIAL_DEVICE;

// This is the GDT, it only contains 3 entries:
//  _ The NULL entry, it is mandatory,
//  _ One code segment spanning the entire address space.
//  _ One data segment spanning the entire address space.
DECLARE_GDT(GDT, 2)

void
__handle_serial_int(struct interrupt_desc_t const * const desc) {
    // Handle interrupt from serial device.
    ASSERT(desc);
    struct char_dev_t * dev = &SERIAL_DEVICE.dev;
    uint8_t buf[1];
    dev->read(dev,buf,1);
    LOG("%c",buf[0]);
}

// Refresh the segment registers and use the new code and data segments.
// After this function returns:
//      CS = `code_seg`
//      DS = SS = ES = FS = GS = `data_seg`.
static void
__refresh_segment_registers(uint16_t const code_seg, uint16_t const data_seg) {
    LOG("Use segments: Code = %x, Data = %x\n", code_seg, data_seg);
    write_cs(code_seg);
    write_ds(data_seg);
    write_ss(data_seg);
    write_es(data_seg);
    write_fs(data_seg);
    write_gs(data_seg);
}

// The code segment of the kernel is at index 1 in the GDT while the data
// segment is at index 2.
static uint16_t const CODE_SEGMENT_IDX = 1;
static uint16_t const DATA_SEGMENT_IDX = 2;

// Right after the GDT is set up, we will load those two selectors into the
// segment registers.
static uint16_t const CODE_SEGMENT_SELECTOR = CODE_SEGMENT_IDX << 3 | RING_0;
static uint16_t const DATA_SEGMENT_SELECTOR = DATA_SEGMENT_IDX << 3 | RING_0;

void
initialize_gdt(void) {
    // Create a flat segmentation model.
    struct segment_desc_t flat_seg = {
        .base = 0x0,
        .size = 0xFFFFF,
        .type = SEGMENT_TYPE_CODE,
        .priv_level = SEGMENT_PRIV_LEVEL_RING0,
    };

    // Create the flat code segment.
    gdt_add_segment(&GDT, CODE_SEGMENT_IDX, &flat_seg);
    // Create the flat data segment.
    flat_seg.type = SEGMENT_TYPE_DATA;
    gdt_add_segment(&GDT, DATA_SEGMENT_IDX, &flat_seg);
}

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

// Print boot banner.
static void
__print_banner(void) {
    LOG("\n");
    LOG("==================================================================\n");
    LOG("Kernel info:\n");
    LOG("    KERNEL_START         = %x\n", KERNEL_START);
    LOG("    SECTION_TEXT_START   = %x\n", SECTION_TEXT_START);
    LOG("    SECTION_TEXT_END     = %x\n", SECTION_TEXT_END);
    LOG("    SECTION_RODATA_START = %x\n", SECTION_RODATA_START);
    LOG("    SECTION_RODATA_END   = %x\n", SECTION_RODATA_END);
    LOG("    SECTION_DATA_START   = %x\n", SECTION_DATA_START);
    LOG("    SECTION_DATA_END     = %x\n", SECTION_DATA_END);
    LOG("    SECTION_BSS_START    = %x\n", SECTION_BSS_START);
    LOG("    SECTION_BSS_END      = %x\n", SECTION_BSS_END);
    LOG("    KERNEL_END           = %x\n", KERNEL_END);
    LOG("    KERNEL_SIZE          = %d bytes.\n", KERNEL_SIZE);
    LOG("    SECTION_TEXT_SIZE    = %d bytes.\n", SECTION_TEXT_SIZE);
    LOG("    SECTION_RODATA_SIZE  = %d bytes.\n", SECTION_RODATA_SIZE);
    LOG("    SECTION_DATA_SIZE    = %d bytes.\n", SECTION_DATA_SIZE);
    LOG("    SECTION_BSS_SIZE     = %d bytes.\n", SECTION_BSS_SIZE);
    LOG("==================================================================\n");
}

void
kernel_main(struct multiboot_info_t const * const multiboot_info) {
    ASSERT(multiboot_info);
    // TODO: command line parsing is disabled until page fault is handled
    // correctly or a mechanism to read from physical memory is implemented.
    //char const * const cmdline = (char const * const)(multiboot_info->cmdline);
    //cmdline_parse(cmdline);

    // If the kernel has been started with the --wait flag then loop until gdb
    // attaches to the virtual machine and resumes the execution. This is very
    // useful since we cannot set a breakpoint on the main before starting the
    // VM.
    while(CMDLINE_PARAMS.wait_start) {
        asm("pause");
    }

    // Initialize the serial console and the tty. We want to do this before
    // anything else so that we can have logs. The following functions
    // (serial_init_dev and tty_init) should not be a problem.
    uint16_t const serial_port = 0x3F8;
    serial_init_dev(&SERIAL_DEVICE, serial_port);
    tty_init(((struct char_dev_t *)&SERIAL_DEVICE));
    // Print the banner.
    __print_banner();

    // Check that all the assumptions made during the development of this kernel
    // hold on the current machine. If not, it will panic.
    assumptions_check();

    // Setup the memory address spaces (segments and paging).
    paging_init();

    // Initialize the GDT and create a flat segmentation model.
    gdt_init(&GDT);
    initialize_gdt();
    // Load the GDT and update the segment registers accordingly.
    gdt_load(&GDT);
    __refresh_segment_registers(CODE_SEGMENT_SELECTOR, DATA_SEGMENT_SELECTOR);

    // Finally setup interrupts.
    interrupt_init();

    // Add an example interrupt handler for interrupt 33.
    interrupt_register_ext_vec(4, 0x21, __handle_serial_int);
    interrupt_enable();

    //apic_start_periodic_timer(33);

    paging_dump_pagedir();
    // Lock up the computer.
    while(1);
}
