#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <string/string.h>
#include <asm/asm.h>
#include <memory/gdt.h>
#include <interrupt/interrupt.h>
#include <interrupt/handlers.h>
#include <includes/debug.h>
#include <asm/cpuid/cpuid.h>
#include <io/serial/serial.h>

// Check all the assumptions we are making in this kernel. At least the ones
// that are checkable at boot time.
void
assumptions_check(void) {
    // We are using the CPUID extensively, as such we only support processor
    // that have this feature. This is done by checking if we can flip the bit
    // 21 in EFLAGS.
    uint16_t const eflags = read_eflags();
    uint16_t const eflags_no_21 = eflags&(~(1<<21));
    uint16_t const bit21 = (eflags&(1<<21))>>21;
    uint16_t const new_eflags = eflags_no_21|((~bit21)<<21);
    write_eflags(new_eflags);
    ASSERT(read_eflags()==new_eflags);
    write_eflags(eflags);

    // For now (or maybe forever), we are supporting Intel processors only. Read
    // the vendorId to make sure.
    char eax[5],ebx[5],ecx[5],edx[5];
    cpuid(0x0,0x0,(uint32_t*)eax,(uint32_t*)ebx,(uint32_t*)ecx,(uint32_t*)edx);
    eax[4] = 0;
    ebx[4] = 0;
    ecx[4] = 0;
    edx[4] = 0;
    ASSERT(streq(ebx,"Genu"));
    ASSERT(streq(edx,"ineI"));
    ASSERT(streq(ecx,"ntel"));
}

void
kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    bool i = 0;
    while(i);
    vga_init();
    tty_init();
    serial_init();
    assumptions_check();

    gdt_init();
    idt_init();

    // Add an example interrupt handler for interrupt 33.
    struct interrupt_gate_t handler33 = {
        .vector = 33,
        .offset = interrupt_handler_33,
        .segment_selector = 0x8,
    };
    idt_register_handler(&handler33);

    // It is important that we disable the legacy PIC before enabling
    // interrupts since it can send interrupts sharing the same vectors as
    // regular exceptions, thus causing problems.
    pic_disable();
    interrupts_enable();

    // Test a `int $21`. This should print "Interrupt 33 received".
    send_int();

    while(1)
        serial_printf("%c",serial_readc());
}
