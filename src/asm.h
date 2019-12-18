// Prototype of functions written in assembly.
#pragma once
#include <types.h>

// Read the Model-Specific-Register into dest.
void
read_msr(uint8_t const msr_num, uint64_t const * dest);

// Write in an MSR the content of val
void
write_msr(uint8_t const msr_num, uint64_t const val);

// Read the Timestamp counter register into *hi and *low.
void
read_tsc(uint32_t * const hi, uint32_t * const low);

void
pic_disable(void);

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

uint32_t
read_eflags(void);
void
write_eflags(uint32_t eflags);

void
outb(uint16_t const port, uint8_t const data);

uint8_t
inb(uint16_t const port);

uint32_t
read_cr0(void);

uint32_t
read_cr3(void);
