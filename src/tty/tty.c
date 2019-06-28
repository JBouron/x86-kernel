#include <tty/tty.h>
#include <string/string.h>

// The tty keep the cursor position in the VGA text matrix.
static uint8_t _tty_cur_pos_x;
static uint8_t _tty_cur_pos_y;

// The tty also handle colors and keep the current foreground and background
// colors in memory.
static enum vga_color_t _tty_cur_fg_color;
static enum vga_color_t _tty_cur_bg_color;

void
tty_init(void) {
    // We set the cursor position to (0,0) in the VGA matrix.
    _tty_cur_pos_x = 0;
    _tty_cur_pos_y = 0;
    // We use light grey on black by default.
    _tty_cur_fg_color = VGA_COLOR_LIGHT_GREY;
    _tty_cur_bg_color = VGA_COLOR_BLACK;
}

static void
_tty_put_char(const char c) {
    if (c == '\n') {
        // When dealing with a new-line character we simply go to the next line
        // in the VGA matrix.
        _tty_cur_pos_y ++;
        _tty_cur_pos_x = 0;
    } else {
        // For every other char we call the vga_put_char with the current cursor
        // position and colors.
        vga_color_desc_t const c_desc =
            vga_create_color_desc(_tty_cur_fg_color,_tty_cur_bg_color);
        vga_char_t const ch = vga_create_char(c,c_desc);
        vga_put_char(ch,_tty_cur_pos_x,_tty_cur_pos_y);

        // We now update the cursor position. If we reached the end of the line
        // go to the next one.
        _tty_cur_pos_x ++;
        if (_tty_cur_pos_x == VGA_WIDTH) {
            _tty_cur_pos_y ++;
            _tty_cur_pos_x = 0;
        }
    }
}

void
tty_print(const char *const str) {
    size_t const len = strlen(str);
    for(size_t i = 0; i < len; ++i) {
        char const cur_char = str[i];
        _tty_put_char(cur_char);
    }
}

void
tty_set_fg_color(enum vga_color_t const color) {
    _tty_cur_fg_color = color;
}

void
tty_set_bg_color(enum vga_color_t const color) {
    _tty_cur_bg_color = color;
}
