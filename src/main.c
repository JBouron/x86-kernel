#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <string/string.h>

void
kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    bool i = 1;
    while(!i);
    vga_init();
    tty_init();
    uint32_t const big = (uint32_t)(-1);
    tty_printf("Test %x string%%: %p and %x\n",big, &big, 54);
}
