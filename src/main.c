#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <string/string.h>
#include <asm/asm.h>
#include <memory/gdt.h>

void
kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    bool i = 1;
    while(!i);
    vga_init();
    tty_init();
    gdt_init();

    tty_printf("END\n");
}
