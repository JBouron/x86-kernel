#pragma once
#include <types.h>

// CPU related operations.

// Read a Model-Specific-Register into dest.
// @param msr_num: The MSR number, as found in Intel's documentation.
// @return: The value read from the MSR.
uint64_t read_msr(uint8_t const msr_num);

// Write in an MSR.
// @param msr_num: The MSR number, as found in Intel's documentation.
// @param val: The value to write into the MSR.
void write_msr(uint8_t const msr_num, uint64_t const val);

// Read the Timestamp counter.
// @return: The value of the TSC
uint64_t read_tsc(void);

// Read the value of the EFLAGS register.
// @return: The value of EFLAGS.
uint32_t read_eflags(void);

// Write into EFLAGS register.
// @param eflags: The value to write into EFLAGS.
void write_eflags(uint32_t eflags);


// Lock up the cpu by disabling the interrupts and halting.
void lock_up(void);
