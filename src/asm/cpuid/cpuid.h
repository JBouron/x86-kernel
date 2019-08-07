// Wrapper for the cpuid instruction as well as helper functions to identify
// features more easily.
#ifndef _CPUID_H
#define _CPUID_H
#include <utils/types.h>

// This is where the magic happens. This function expects all pointers to be
// valid.
void
__cpuid(uint32_t in_eax,
        uint32_t in_ecx,
        // Output:
        uint32_t *out_eax,
        uint32_t *out_ebx,
        uint32_t *out_ecx,
        uint32_t *out_edx);

// Execute the CPUID instruction using in_eax and in_ecx as %eax and %ecx
// respectively.
// Write the resulting eax, ebx, ecx and edx in out_*.
// The out_* pointers can be left NULL. In this case the result of the
// corresponding register will be discarded.
static inline void
cpuid(uint32_t in_eax,
      uint32_t in_ecx,
      // Output:
      uint32_t *out_eax,
      uint32_t *out_ebx,
      uint32_t *out_ecx,
      uint32_t *out_edx) {
    // Those addresses will be used for the __cpuid call.
    uint32_t _out_eax, _out_ebx, _out_ecx, _out_edx;
    __cpuid(in_eax, in_ecx, &_out_eax, &_out_ebx, &_out_ecx, &_out_edx);

    // Now write in the out_* pointers, at least in the non-NULL ones.
    if (out_eax) *out_eax = _out_eax;
    if (out_ebx) *out_ebx = _out_ebx;
    if (out_ecx) *out_ecx = _out_ecx;
    if (out_edx) *out_edx = _out_edx;
}
#endif
