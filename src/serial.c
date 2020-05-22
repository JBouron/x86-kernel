#include <serial.h>
#include <cpu.h>
#include <debug.h>

// We assume that the COM port is located at this port addresses. There should
// be a test/assumption check for this.
// This global is not marked const for testing purposes.
static uint16_t COM = 0x3F8;

// These functions allow the testing functions to use other functions that
// cpu_outb and cpu_inb for writing and reading to/from the serial controller.
static void (*WRITE_BYTE)(uint16_t, uint8_t) = cpu_outb;
static uint8_t (*READ_BYTE)(uint16_t) = cpu_inb;

// The maximum baud rate.
#define MAX_BAUD 115200

// Memnotics for the serial registers.
enum register_t {
    DATA = 0,
    INT_ENABLE = 1,
    INT_ID = 2,
    LINE_CTRL = 3,
    MODEM_CTRL = 4,
    LINE_STATUS = 5,
    MODEM_STATUS = 6,
    SCRATCH = 7,
};

// Write a value into one of COM's registers.
// @param reg: The register to write into.
// @param value: The value to write into the register.
static void write_register(enum register_t const reg, uint8_t const value) {
    uint16_t const port = COM + (uint16_t) reg;
    WRITE_BYTE(port, value);
}

// Read one of COM's register.
// @param reg: Which register to read from.
// @return: The current value of the register `reg`.
static uint8_t read_register(enum register_t const reg) {
    uint16_t const port = COM + (uint16_t) reg;
    return READ_BYTE(port);
}

// This summarize the status of the serial controller.
union status_t {
    uint8_t value;
    struct {
        // Set if there is data that can be read.
        uint8_t data_ready : 1;
        // Set if there has been data lost.
        uint8_t overrun_error : 1;
        // Set if a parity error has been detected during transmission.
        uint8_t parity_error : 1;
        // Set if a stop bit was missing.
        uint8_t framing_error : 1;
        // Set if ther is a break in data input.
        uint8_t break_indicator : 1;
        // Set if the transmission buffer is empty (eg. data can be sent).
        uint8_t trans_buf_empty : 1;
        // Set if the transmitter is not doing anything.
        uint8_t transmitter_empty : 1;
        // Set if there is an error with a word in the input buffer.
        uint8_t impending_error : 1;
    };
} __attribute__((packed));
// The status struct should be a single byte so that casting from/to uint8_t is
// easy.
STATIC_ASSERT(sizeof(union status_t) == 1, "struct status should be 1 byte");

// Get the status of the serial controller.
// @return: Current status in a struct status format.
static union status_t get_status(void) {
    uint8_t const line_status_reg = read_register(LINE_STATUS);
    union status_t const status = {
        .value = line_status_reg
    };
    return status;
}

// Modify the DLAB bit.
// @param dlab: The value to write in the DLAB bit.
static void dlab_is(bool const dlab) {
    uint8_t const val = read_register(LINE_CTRL);
    // DLAB is the most significant bit of the Line Control register.
    uint8_t const new_val = dlab ? val | 0x80 : val & 0x7F;
    write_register(LINE_CTRL, new_val);
}

// Set the baud rate to a specific value. This function will compute the divisor
// and write the corresponding registers.
// @param rate: The target baud rate.
static void set_baud_rate(uint32_t const rate) {
    ASSERT(0 < rate && rate <= MAX_BAUD);
    // The divisor is represented with 16 bits, therefore the rate has a lower
    // bound equal to MAX_BAUD / 64535 = 1.7 -> 2.
    if (rate < 2) {
        PANIC("Requested Baud rate is too small\n");
    }
    uint16_t const divisor = MAX_BAUD / rate;
    // The least significant byte of the divisor should be written into the DATA
    // register, while the most significant one should be written into the
    // INT_ENABLE. Both write should be performed with the DLAB bit set.
    dlab_is(true);
    write_register(DATA, divisor & 0xF);
    write_register(INT_ENABLE, divisor >> 8);
    dlab_is(false);
}

// Set the character length to be used with the serial port.
// @param length: The character to use. Must be 5 <= length <= 8.
static void set_char_length(uint8_t const length) {
    // The character length can only be between 5 and 8.
    ASSERT(5 <= length && length <= 8);
    uint8_t const val = length - 5;
    
    // The character length is set by writing the 2 least significant bits of
    // the line control register.
    uint8_t const ln_val = read_register(LINE_CTRL);
    uint8_t const ln_new = (ln_val & 0xFC) | val;
    write_register(LINE_CTRL, ln_new);
}

// Select the number of stops bits.
// @param count: Number of stop bit to use for subsequent use of the serial
// port. Must be either 1 or 2.
static void set_stop_bits(uint8_t const count) {
    ASSERT(1 <= count && count <= 2);
    uint8_t const stop_bit = count << 1;
    uint8_t const ln_val = read_register(LINE_CTRL);
    uint8_t const ln_new = (ln_val & 0xFB) | stop_bit;
    write_register(LINE_CTRL, ln_new);
}

// Parity mode for serial communcation.
enum parity_t {
    NONE = 0x0,
    ODD = 0x1,
    EVEN = 0x3,
    MARK = 0x5,
    SPACE = 0x7,
};

// Set the parity for the serial port.
// @param parity: The parity mode to use.
static void set_parity(enum parity_t const parity) {
    // Parity is written in bit 3, 4, and 5 of the line control register.
    uint8_t const ln_val = read_register(LINE_CTRL);
    uint8_t const ln_new = (ln_val & 0xC7) | ((uint8_t)parity << 3);
    write_register(LINE_CTRL, ln_new);
}

// Enable the interrupts to be sent by the serial controller when data is ready
// to be read.
static void enable_serial_interrupt(void) {
    write_register(INT_ENABLE, 1);
}

// Implementation of the write interface of the SERIAL_STREAM I/O stream.
// @param buf: The buffer to write to the serial port.
// @param len: Length of the buffer.
// @return: The number of bytes successfully sent.
static size_t serial_write(uint8_t const * const buf, size_t const len) {
    size_t sent = 0;
    for (size_t i = 0; i < len && get_status().trans_buf_empty; ++i, ++sent) {
        // Write as much as we can in the data register.
        write_register(DATA, buf[i]);
        if (buf[i] == '\n') {
            // To get new line on the other side of the serial port we need to
            // output a carriage return as well. One tricky part in doing this
            // is that we might send more data than requested. For now we hide
            // this from the callee by return at most `len`.
            write_register(DATA, '\r');
        }
    }
    return sent;
}

// Read from the serial controller.
// @param buf: The buffer to read into.
// @param len: The number of bytes to read.
// @return: The number of bytes read into buf.
static size_t serial_read(uint8_t * const buf, size_t const len) {
    size_t i = 0;
    for (; i < len && get_status().data_ready; ++i) {
        buf[i] = read_register(DATA);
    }
    return i;
}

// The serial I/O stream.
struct io_stream SERIAL_STREAM = {
    .read = serial_read,
    .write = serial_write,
};

// Initialize the serial stream.
void serial_init(void) {
    set_baud_rate(38400);
    set_char_length(8);
    set_stop_bits(1);
    set_parity(NONE);
    enable_serial_interrupt();
}

#include <serial.test>
