#include <vga/vga.h>

void
vga_init(void) {
    // Clear the VGA matrix by writing empty char (black on black). This is
    // needed as the VGA matrix is used by the BIOS before the kernel is
    // started and thus we might see garbage.
    vga_clear_matrix();
}

void
vga_clear_matrix(void) {
    vga_color_desc_t const clr = vga_create_color_desc(VGA_COLOR_BLACK,
                                                       VGA_COLOR_BLACK);
    vga_char_t const empty_char = vga_create_char(' ', clr);
    for (uint8_t y = 0; y < VGA_HEIGHT; ++y) {
        for (uint8_t x = 0; x < VGA_WIDTH; ++x) {
            vga_put_char(empty_char, x, y);
        }
    }
}

vga_color_desc_t
vga_create_color_desc(enum vga_color_t const fg, enum vga_color_t const bg) {
    // The pair of color making the color desc is simply an encoding in a
    // uint8_t where the foreground color is in the 4 LSBs while the background
    // color is in the 4 MSBs.
    return fg | (bg<<4);
}

vga_char_t
vga_create_char(unsigned char const c, vga_color_desc_t const color_desc) {
    // The VGA char is simply a uint16_t where the character is located in the
    // lower 8 bits and the color description is located in the higher 8 bits.
    return c | (color_desc<<8);
}

void
vga_put_char(vga_char_t const vga_char, uint8_t x, uint8_t y) {
    // The VGA matrix is a single array in the memory, thus we have to compute
    // the linear index corresponding to the position (x,y).
    uint16_t const linear_idx = x + y * VGA_WIDTH;
    // We assume that the VGA buffer stays at the same address after setting up
    // paging. This can be done using idempotent mapping for this memory area.
    uint16_t* const vga_buf = VGA_BUFFER_OFFSET;
    vga_buf[linear_idx] = vga_char;
}
