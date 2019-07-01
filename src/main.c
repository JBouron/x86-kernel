#include <includes/types.h>
#include <vga/vga.h>
#include <tty/tty.h>
#include <string/string.h>
#include <asm/asm.h>

void
kernel_main(void) {
    // Initialize the VGA text buffer and the TTY.
    bool i = 1;
    while(!i);
    vga_init();
    tty_init();

    // IA32_APIC_BASE.
    uint32_t const msr_nr = 0x10;
    uint32_t hi_msr = 0, lo_msr = 0, hi_tsc = 0, lo_tsc = 0;
    read_msr(msr_nr,&hi_msr,&lo_msr);
    read_tsc(&hi_tsc,&lo_tsc);

    tty_printf("MSR : %x %x\n",hi_msr,lo_msr);
    tty_printf("TSC : %x %x\n",hi_tsc,lo_tsc);
}
