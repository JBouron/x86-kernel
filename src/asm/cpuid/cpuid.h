// Wrapper for the cpuid instruction as well as helper functions to identify
// features more easily.
#ifndef _CPUID_H
#define _CPUID_H
// Execute the CPUID instruction using in_eax and in_ecx as %eax and %ecx
// respectively.
// Write the resulting eax, ebx, ecx and edx in out_*.
void
cpuid(uint32_t in_eax,uint32_t in_ecx,
      uint32_t *out_eax,uint32_t *out_ebx,uint32_t *out_ecx,uint32_t *out_edx);
#endif
