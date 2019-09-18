#include <tty/tty.h>
#include <utils/string.h>
#include <utils/memory.h>
#include <utils/generic_printf/generic_printf.h>

struct tty TTY;

void
tty_init(struct char_dev * const cdev) {
    TTY.dev = cdev;
}

void
tty_putc(const char c) {
    // Write one char into the driver.
    uint8_t buf[] = {c};
    struct char_dev * const dev = TTY.dev;
    dev->write(dev, buf, 1);
}

void
tty_printf(const char * const fmt, ...) {
    // We use the generic_printf function with the tty_putc function as backend.
    va_list list;
    va_start(list, fmt);
    generic_printf(tty_putc, fmt, list);
    va_end(list);
}
