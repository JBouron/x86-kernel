// Functions, types and constants used to interact with the VGA text matrix.
#ifndef _VGA_H
#define _VGA_H
#include <includes/types.h>

// The location of the VGA buffer in memory. Note this is before paging is
// enabled.
static uint16_t * const VGA_BUFFER_OFFSET = (uint16_t*)0xB8000;
// The width and height of the VGA text matrix.
static size_t const VGA_WIDTH = 80;
static size_t const VGA_HEIGHT = 25;

// The VGA text mode color codes. These can be used both for foreground and
// background colors.
enum vga_color_t {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

// Initialize the VGA text matrix.
void
vga_init(void);

// Clear the VGA matrix with a black background.
void
vga_clear_matrix(void);

// vga_color_desc_t are used to colorize the text in the VGA buffer. It is
// composed of a pair of color, one for the foreground and the other for the
// background.
typedef uint8_t vga_color_desc_t;

// Create a VGA color pair (<foreground>, <background>).
vga_color_desc_t 
vga_create_color_desc(enum vga_color_t const fg, enum vga_color_t const bg);

// Each cell in the VGA matrix is a uint16_t containing the value of the
// character and it's color description in the form: <char> | <desc> << 8.
typedef uint16_t vga_char_t;
// Create a VGA char from a character code and a color description.
vga_char_t
vga_create_char(unsigned char const c, vga_color_desc_t const color_desc);

// Write a VGA char in the VGA matrix at position (x, y).
void
vga_put_char(vga_char_t const vga_char, uint8_t x, uint8_t y);
#endif
