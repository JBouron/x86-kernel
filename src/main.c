#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <string/string.h>

void kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    int i = 1;
    while(!i);
    vga_init();
    tty_init();
    uint32_t const big = 300000000;
    tty_printf("Test %d string: %d and %d\n",big, 1010, 54);
}
