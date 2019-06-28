#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>

void kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    vga_init();
    tty_init();
	tty_print("Hello world!\n");
    tty_set_fg_color(VGA_COLOR_GREEN);
	tty_print("What a wonderful day\n");
    tty_set_fg_color(VGA_COLOR_LIGHT_MAGENTA);
    tty_set_bg_color(VGA_COLOR_BLUE);
	tty_print("L33T\n");
}
