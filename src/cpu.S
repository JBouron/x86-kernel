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

ASM_FUNC_DEF(has_cpuid):
    // From IA-32 manual:
    // "The ID flag (bit 21) in the EFLAGS register indicates support for the
    // CPUID instruction. If a software procedure can set and clear this flag,
    // the processor executing the procedure supports the CPUID instruction."
    pushfl
    // Invert the value of the ID bit in EFLAGS.
    xorl      $0x200000, (%esp)
    // Save value of EFLAGS with inverted ID bit.
    mov     (%esp), %eax
    // Reset EFLAGS with the new value having the inverted ID bit.
    popfl
    // Re-read EFLAGS.
    pushfl
    // Compare with the saved value in %eax, if CPUID is supported then the
    // inversion of the ID bit is successful and the value of EFLAGS just read
    // is the same as the saved value in %eax.
    cmp     (%esp), %eax
    jne     different
    // %eax and EFLAGS are the same, inversion was sucessful.
    mov     $1, %eax
    jmp     end
different:
    // %eax and EFLAGS are different, inversion was not sucessful, return false.
    mov     $0, %eax
end:
    // Discard value in top of stack.
    add     $0x4, %esp
    ret 

// void cpuid_ecx(uint32_t i_eax, uint32_t i_ecx,
//                uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx,
//                uint32_t *o_edx);
ASM_FUNC_DEF(cpuid_ecx):
    push    %ebp
    mov     %esp, %ebp
    
    // Write the value of EAX and ECX.
    mov     0x8(%esp), %eax
    mov     0xC(%esp), %ecx

    // Exectute CPUID. Save %ebx into the stack before hand.
    push    %ebx 
    cpuid

    // Push the output of CPUID onto the stack.
    push    %eax
    push    %ebx
    push    %ecx
    push    %edx

    // For any non null destination write the value of the corresponding
    // register.
    // Make %eax point onto the last argument of the function ie. o_edx.
    lea     28(%ebp), %eax
loop_start:
    mov     (%eax), %ecx
    cmp     $0, %ecx
    je      skip_null
    // The pointer is non null, store the value.
    mov     (%esp), %edx
    mov     %edx, (%ecx)
skip_null:
    // Remove output value from the stack.
    add     $0x4, %esp
    // Go to next address in the argument list.
    sub     $0x4, %eax
    lea     0xC(%ebp), %ecx
    cmp     %eax, %ecx
    jne     loop_start

    movl     $0, 54(,%eax,4)

    // And we are done.
    // Restore EBX.
    pop     %ebx
    pop     %ebp
    ret

//void cpuid(uint32_t i_eax, uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx,
//           uint32_t *o_edx);
ASM_FUNC_DEF(cpuid):
    // Since ECX is not provided in argument we need to use 0 and call
    // cpuid_ecx.
    // Make %eax point to the last argument, that is the higher argument in the
    // stack. There are 5 args in cpuid(), hence this is %esp + 4 * 5.
    lea     20(%esp), %eax
    // Now push all the arguments necessary to call cpuid_eax(). Starting will
    // the four output addresses.
    pushl   (%eax)
    pushl   -0x4(%eax)
    pushl   -0x8(%eax)
    pushl   -0xC(%eax)
    // The value to be used for ECX, 0 in this case since it is not given.
    pushl   $0 
    // And the value of EAX.
    push    -0x10(%eax)
    // All arguments are pushed, we can call cpuid_eax.
    call    cpuid_ecx
    add     $24, %esp
    ret
