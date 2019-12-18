#include <macro.h>

// uint64_t read_msr(uint8_t const msr_num);
ASM_FUNC_DEF(read_msr):
    // rdmsr gets the MSR number from %ecx and returns the content in %edx:%eax.
    mov     0x4(%esp), %ecx
    rdmsr
    // Since %edx:%eax already contain the MSR value, no need to do anything.
    ret

// void write_msr(uint8_t const msr_num, uint64_t const val);
ASM_FUNC_DEF(write_msr):
    // Stack contains:
    //  |   val hi     | + 0xC
    //  |   val low    | + 0x8
    //  |   msr_num    | + 0x4
    //  |   ret addr   | <- esp
    mov     0x4(%esp), %ecx
    mov     0xC(%esp), %edx
    mov     0x8(%esp), %eax
    // WRMSR writes %edx:%eax into MSR %ecx.
    wrmsr
    ret

ASM_FUNC_DEF(read_tsc):
    // RDTSC reads the TCS into %edx:%eax.
    rdtsc
    ret

ASM_FUNC_DEF(read_eflags):
    pushf
    pop     %eax
    ret

ASM_FUNC_DEF(write_eflags):
    mov     0x4(%esp), %eax
    push    %eax
    popf
    ret

ASM_FUNC_DEF(lock_up):
0:
    cli
    hlt
    // In case we ever wake up from the halt, jump to the beginning and go back
    // to locked up mode.
    jmp     0b
