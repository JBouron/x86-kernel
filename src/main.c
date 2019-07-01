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
    for(uint32_t y = 0; y < VGA_HEIGHT; ++y) {
        tty_printf("Line %u ########################################################################################################\n",y);
    }
}
