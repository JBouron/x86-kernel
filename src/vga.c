#include <vga.h>
#include <memory.h>
#include <kernel_map.h>

#define VGA_DEFAULT_MATRIX_ADDR 0xB8000

// The location of the VGA buffer in memory. This address can be changed through
// the vga_set_buffer_addr function.
static uint16_t * MATRIX_ADDR = (uint16_t*)VGA_DEFAULT_MATRIX_ADDR;

// The width and height of the VGA text matrix. Those are not made const so that
// it is possible to test with custom size.
static uint8_t VGA_WIDTH = 80;
static uint8_t VGA_HEIGHT = 25;

// The VGA text mode color codes. These can be used both for foreground and
// background colors.
enum color_t {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHT_GREY = 7,
    DARK_GREY = 8,
    LIGHT_BLUE = 9,
    LIGHT_GREEN = 10,
    LIGHT_CYAN = 11,
    LIGHT_RED = 12,
    LIGHT_MAGENTA = 13,
    LIGHT_BROWN = 14,
    WHITE = 15,
};

// color_desc_t are used to colorize the text in the VGA buffer. It is
// composed of a pair of color, one for the foreground and the other for the
// background.
typedef uint8_t color_desc_t;

// Each cell in the VGA matrix is a uint16_t containing the value of the
// character and it's color description in the form: <char> | <desc> << 8.
typedef uint16_t char_t;

// Create a color_desc_t from a foreground and background color pair.
// @param fg: The foreground color.
// @param bg: The background color.
// @return: A color_desc_t encoding the color pair <foreground,background>.
static color_desc_t create_color_desc(enum color_t const fg,
                                      enum color_t const bg) {
    // The pair of color making the color desc is simply an encoding in a
    // uint8_t where the foreground color is in the 4 LSBs while the background
    // color is in the 4 MSBs.
    return fg | (bg << 4);
}

// Create a VGA character.
// @param chr: The glyph to be used for the VGA character.
// @param color: The color desc containing both the foreground and background
// color.
// @return: A VGA character encoding the glyph, foreground color and background
// color.
static char_t create_char(unsigned char const chr, color_desc_t const color) {
    // The VGA char is simply a uint16_t where the character is located in the
    // lower 8 bits and the color description is located in the higher 8 bits.
    return chr | (color << 8);
}

// Put a VGA character in the VGA matrix at given coordinates.
// @param c: The VGA character to output.
// @param x: The x coordinate in the matrix to output the character to.
// @param y: The y coordinate in the matrix to output the character to.
// Note: The (0;0) coordinate indicate the top-left corner of the VGA matrix.
static void put_char_at(char_t const c, uint8_t const x, uint8_t const y) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        // Bogus coordinates. Do nothing.
        return;
    }
    // The VGA matrix is a single array in the memory, thus we have to compute
    // the linear index corresponding to the position (x, y).
    uint16_t const linear_idx = x + y * VGA_WIDTH;
    // We assume that the VGA buffer stays at the same address after setting up
    // paging. This can be done using idempotent mapping for this memory area.
    uint16_t* const vga_buf = MATRIX_ADDR;
    vga_buf[linear_idx] = c;
}

// Clear the VGA matrix with a black color.
static void clear_matrix(void) {
    // Memseting the matrix to 0 works as it describes a black foreground on a
    // black background with a \0 char.
    size_t const matrix_size = VGA_WIDTH * VGA_HEIGHT * 2;
    memzero((uint8_t*)MATRIX_ADDR, matrix_size);
}

// We use a linear indexing of the VGA matrix. In memory the matrix is
// represented in a row-major order.
// By default put the cursor on the top-left corner of the matrix.
static uint16_t cursor = 0;
// The maximum cursor position. If the cursor is currently at this value then
// the cursor is outside of the buffer.
#define cursor_max (VGA_WIDTH * VGA_HEIGHT)

// Scroll up the content of the VGA matrix.
static void scroll_up(void) {
    // Memcpy from the second row to the first row. In total we copy the
    // entirety of the matrix minus one row.
    uint16_t * const dst = MATRIX_ADDR;
    uint16_t const * const src = MATRIX_ADDR + VGA_WIDTH;
    size_t const cpy_size = VGA_WIDTH * (VGA_HEIGHT - 1) * 2;
    memcpy(dst, src, cpy_size);

    // Clear the last line to avoid leaving garbage behind.
    uint16_t * const last_row = MATRIX_ADDR + VGA_WIDTH * (VGA_HEIGHT - 1);
    size_t const row_size = VGA_WIDTH * 2;
    memzero((uint8_t*)last_row, row_size);
}

// Handle a new line character coming to the VGA input stream.
static void handle_newline(void) {
    // A newline is simply setting the cursor to the next multiple of VGA_WIDTH.
    if (cursor > 0 && cursor % VGA_WIDTH == 0 && MATRIX_ADDR[cursor-1]) {
        // The cursor is on the beginning of a new line which hapenned because
        // of a line wrap. In this case we can silently drop the newline
        // character as the string printed fits exactly in one row.
        // For example with a matrix 2x2 and string "AB\n":
        //  A B
        //  . .
        //  ^---- When "printing" \n the cursor is here, but since the line has
        //  been wrapped already we don't _really_ need to print the newline.
        // This avoid awkward double-newline if a string has the same size as a
        // row (minus the new line).
        return;
    }
    uint8_t const line = cursor / VGA_WIDTH;
    if (line == VGA_HEIGHT - 1) {
        // The cursor is on the last line. We need to scroll up and set cursor
        // to the previous multiple of VGA_WIDTH.
        scroll_up();
        cursor = line * VGA_WIDTH;
    } else {
        cursor = (line + 1) * VGA_WIDTH;
    }
}

// Print out a character under the cursor in the VGA matrix.
// @param chr: The character to be printed out.
static void handle_char(char const chr) {
    if (chr == '\n') {
        // Treat new lines character as a special case.
        handle_newline();
    } else {
        // This is a regular character. Write the character under the cursor and
        // move the cursor to the next position.
        if (cursor == cursor_max) {
            // We reached the end of the last line. The logic is essentially the
            // same as a new line.
            scroll_up();
            // We need the -1 here as the cursor is a the beginning of the first
            // row that is out of bounds. The -1 helps putting back the cursor
            // on the last valid row.
            cursor = ((cursor / VGA_WIDTH) - 1) * VGA_WIDTH;
        }
        color_desc_t const clr = create_color_desc(LIGHT_GREY, BLACK);
        char_t const vga_char = create_char(chr, clr);
        MATRIX_ADDR[cursor] = vga_char;
        // Move the cursor to the right.
        cursor ++;
    }
}

// Write a buffer containing text into the VGA matrix while taking care of
// newlines, line-wraps and scrolling.
// @param buf: A buffer containing a NULL terminated string.
// @param length: The length of the buffer to write into the VGA matrix.
// This function is used by the VGA_STREAM io_stream_t structure used as a
// public interface.
static size_t stream_write(uint8_t const * const buf, size_t const length) {
    for (size_t i = 0; i < length; i++) {
        handle_char(buf[i]);
    }
    return length;
}

// This is the public facing interface to the VGA matrix used to write.
struct io_stream VGA_STREAM = {
    .read = NULL,
    .write = stream_write,
};

// Initialize the VGA matrix.
void vga_init(void) {
    // The <1MiB physical memory is mapped in the higher half with the kernel.
    MATRIX_ADDR = to_virt(MATRIX_ADDR);

    // Clear whatever is left in the VGA matrix from the BIOS/boot sequence.
    clear_matrix();
}

// Set a custom address to use as a VGA buffer.
void vga_set_buffer_addr(uint16_t * const addr) {
    MATRIX_ADDR = addr;
}

// All tests are written in the vga_test.c file which is included here. That way
// we can access static functions without polluting the file.
#include <vga.test>
