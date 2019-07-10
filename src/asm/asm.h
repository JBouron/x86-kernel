// Prototype of functions written in assembly.
#ifndef _ASM_H
#define _ASM_H
#include <memory/segment.h>
// Read the Model-Specific-Register into hi and low.
// Hi will contain the high order 32 bits of the MSR while low will contain the
// low order ones. (ie. *hi = %edx, *low = %eax after the call to rdmsr).
void
read_msr(uint8_t const msr_num, uint32_t * const hi, uint32_t * const low);

// Read the Timestamp counter register into *hi and *low.
void
read_tsc(uint32_t * const hi, uint32_t * const low);

// Disable interrupts.
void
interrupts_disable(void);

// Enable interrupts.
void
interrupts_enable(void);

void
pic_disable(void);

// Load the address gdt into the GDTR register.
void
load_gdt(struct table_desc_t *table_desc);

// Load the address gdt into the IDTR register.
void
load_idt(struct table_desc_t *table_desc);

// Write value `val` in segment registers.
void
write_cs(uint16_t val);
void
write_ds(uint16_t val);
void
write_ss(uint16_t val);
void
write_es(uint16_t val);
void
write_fs(uint16_t val);
void
write_gs(uint16_t val);

void
dummy_interrupt_handler(void);

void
send_int(void);

// Lock up the machine. Interrupts are disabled and the processor enters a halt
// state.
void
lock_up(void);

uint16_t
read_eflags(void);
void
write_eflags(uint16_t eflags);

void
outb(uint16_t const port, uint8_t const data);

uint8_t
inb(uint16_t const port);
#endif
