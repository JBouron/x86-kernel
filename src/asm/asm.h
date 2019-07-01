// Prototype of functions written in assembly.
#ifndef _ASM_H
#define _ASM_H
// Read the Model-Specific-Register into hi and low.
// Hi will contain the high order 32 bits of the MSR while low will contain the
// low order ones. (ie. *hi = %edx, *low = %eax after the call to rdmsr).
void
read_msr(uint8_t const msr_num, uint32_t *const hi, uint32_t *const low);

// Read the Timestamp counter register into *hi and *low.
void
read_tsc(uint32_t *const hi, uint32_t *const low);
#endif
