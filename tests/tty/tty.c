#include <tty/tty.h>
#include <devices/char_device.h>
#include <utils/memory.h>

// Test the tty using a fake char device.

#define BUFFER_SIZE (256)
char BUFFER[BUFFER_SIZE];
// Position of the cursor in the buffer.
static uint32_t pos = 0;

static size_t
fake_write(struct char_dev_t * dev, uint8_t * const buf, size_t const len) {
    for (size_t i = 0; i < len; ++i) {
        BUFFER[pos++] = buf[i];
    }
    return len;
}

static struct char_dev_t fake_dev = {
    .read = NULL,
    .write = fake_write,
};

int main(int argc, char **argv) {
    memzero(BUFFER, BUFFER_SIZE);

    tty_init(&fake_dev);

    // Test putc.
    tty_putc('a');
    tty_putc('b');
    tty_putc('c');
    tty_putc('d');

    if (!streq(BUFFER, "abcd")) {
        return 1;
    }

    // Test printf.
    tty_printf("My uint32_t = %x\n", 0xDEADBEEF);

    if (!streq(BUFFER, "abcdMy uint32_t = 0xDEADBEEF\n")) {
        return 1;
    }

    return 0;
}
