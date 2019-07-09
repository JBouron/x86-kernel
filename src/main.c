#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <string/string.h>
#include <asm/asm.h>
#include <memory/gdt.h>
#include <interrupt/interrupt.h>
#include <interrupt/handlers.h>

void
kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    bool i = 0;
    while(i);
    vga_init();
    tty_init();
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

    tty_printf("END\n");
}
