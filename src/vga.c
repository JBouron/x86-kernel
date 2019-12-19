#include <vga.h>
#include <memory.h>

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
    // Create a VGA character using black for the foreground and background
    // colors.
    color_desc_t const clr = create_color_desc(BLACK, BLACK);
    char_t const empty_char = create_char('\0', clr);

    // Print the above character in the entire matrix.
    for (uint8_t y = 0; y < VGA_HEIGHT; ++y) {
        for (uint8_t x = 0; x < VGA_WIDTH; ++x) {
            put_char_at(empty_char, x, y);
        }
    }
}

// The following variable contain the x and y coordinates of the cursor inside
// the VGA matrix.
// By default the cursor is located at (0;0).
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

// Scroll up the content of the VGA matrix.
static void scroll_up(void) {
    // Scrolling up.
    for (uint8_t i = 0; i < VGA_HEIGHT - 1; ++i) {
        uint16_t const * const src = MATRIX_ADDR + (i + 1) * VGA_WIDTH;
        uint16_t * const dst = MATRIX_ADDR + i * VGA_WIDTH;
        // One line is VGA_WIDTH * 2 bytes since a single cell is 16-bits.
        memcpy(dst, src, VGA_WIDTH * 2);
    }

    // Clear the last line.
    color_desc_t const clr = create_color_desc(BLACK, BLACK);
    char_t const empty_char = create_char('\0', clr);

    for (uint8_t i = 0; i < VGA_WIDTH; ++i) {
        put_char_at(empty_char, i, VGA_HEIGHT - 1);
    }
}

// Handle a new line character coming to the VGA input stream.
static void handle_newline(void) {
    // On newline, increment the y coordinate of the cursor. Make sure not to go
    // over the height limits.
    if (cursor_y < VGA_HEIGHT - 1) {
        cursor_y ++;
    } else {
        // We are at the bottom of the matrix (last line). Hence we need to
        // "scroll up" the matrix to make space.
        scroll_up();
    }
    // A newline also acts as a carriage return, set the x coordinate of the
    // cursor to 0.
    cursor_x = 0;
}

// Wraps the current line of the matrix, that is jump to the beginning of the
// next line.
static void wrap_line(void) {
    // Wrapping a line is essentially the same as having a newline character.
    handle_newline();
}

// Print out a character under the cursor in the VGA matrix.
// @param chr: The character to be printed out.
static void handle_char(char const chr) {
    if (chr == '\n') {
        // Treat new lines character as a special case.
        handle_newline();
    } else {
        // This is a regular character.
        if (cursor_x == VGA_WIDTH) {
            // Note: A character is printed out under the cursor, and then the
            // cursor is moved. However it might happen that the cursor is
            // beyond the end of the matrix row in which case we need to wrap
            // the line.
            // The reason the line wrap is triggered when a character is written
            // beyond the row limits and not a the last valid position in the
            // row is to avoid useless new lines if the row contains exactly the
            // maximum number of character.
            wrap_line();
        }
        // The cursor coordinates are now valid. We can output the character.
        color_desc_t const clr = create_color_desc(LIGHT_GREY, BLACK);
        char_t const vga_char = create_char(chr, clr);
        put_char_at(vga_char, cursor_x, cursor_y);
        // Move the cursor to the right.
        cursor_x ++;
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
struct io_stream_t VGA_STREAM = {
    .read = NULL,
    .write = stream_write,
};

// Initialize the VGA matrix.
void vga_init(void) {
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
