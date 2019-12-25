#pragma once
#include <types.h>

// CPU related operations.

// Read a Model-Specific-Register into dest.
// @param msr_num: The MSR number, as found in Intel's documentation.
// @return: The value read from the MSR.
uint64_t read_msr(uint32_t const msr_num);

// Write in an MSR.
// @param msr_num: The MSR number, as found in Intel's documentation.
// @param val: The value to write into the MSR.
void write_msr(uint32_t const msr_num, uint64_t const val);

// Read the Timestamp counter.
// @return: The value of the TSC
uint64_t read_tsc(void);

// Lock up the cpu by disabling the interrupts and halting.
void lock_up(void);

// Test whether or not the current CPU supports the CPUID instruction. This is
// done using the ID flags in EFLAGS (see manual page 765).
// @return: True iff the CPU supports CPUID.
bool has_cpuid(void);

// Execute the CPUID instruction with a specific value of EAX, ECX is left to 0.
// If ECX is needed, use the cpuid_ecx variant.
// @param i_eax: The value of EAX to use when calling CPUID.
// @param o_eax [out]: The address to write the value of EAX to after, the call
// to CPUID.
// @param o_ebx [out]: The address to write the value of EBX to after, the call
// to CPUID.
// @param o_ecx [out]: The address to write the value of ECX to after, the call
// to CPUID.
// @param o_edx [out]: The address to write the value of EDX to after, the call
// to CPUID.
// Note: Any of the output pointers can be NULL, in which case the value is
// ignored.
void cpuid(uint32_t i_eax,
           uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx, uint32_t *o_edx);

// Execute the CPUID instruction with specific values for EAX and ECX.
// @param i_eax: The value of EAX to use when calling CPUID.
// @param i_ecx: The value of ECX to use when calling CPUID.
// @param o_eax [out]: The address to write the value of EAX to after, the call
// to CPUID.
// @param o_ebx [out]: The address to write the value of EBX to after, the call
// to CPUID.
// @param o_ecx [out]: The address to write the value of ECX to after, the call
// to CPUID.
// @param o_edx [out]: The address to write the value of EDX to after, the call
// to CPUID.
// Note: Any of the output pointers can be NULL, in which case the value is
// ignored.
void cpuid_ecx(uint32_t i_eax, uint32_t i_ecx,
               uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx,
               uint32_t *o_edx);

// Write a byte to an I/O port.
// @param port: The port to write the byte to.
// @param byte: The value of the byte to write to the port.
void cpu_outb(uint16_t const port, uint8_t const byte);

// Write a word to an I/O port.
// @param port: The port to write the word to.
// @param byte: The value of the word to write to the port.
void cpu_outw(uint16_t const port, uint16_t const word);

// Read a byte from an I/O port.
// @param port: The port to read a byte from.
uint8_t cpu_inb(uint16_t const port);

// Execute tests on CPU related functions.
void cpu_test(void);
