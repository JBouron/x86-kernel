#include <asm/macro.h>

// Definition of the cpuid function.
ASM_FUNC_DEF(__cpuid):
    # Args in order: in_eax, in_ecx, out_ebx, out_ecx, out_edx.
    # Offset %ebp:   0x8   ,0xC   ,0x10   ,0x14   ,0x18.
    push    %ebp
    mov     %esp, %ebp
    push    %ebx    # %ebx is not a scratch register.
    mov     0x8(%ebp), %eax
    mov     0xC(%ebp), %ecx
    cpuid
    # Push all the values of %eax, %ebx, %ecx, %edx in the stack. We will pop
    # them into memory afterwards.
    push    %edx
    push    %ecx
    push    %ebx
    push    %eax
    # Pop the values of the register into the destination pointers.
    mov     0x10(%ebp), %eax
    pop     (%eax)
    mov     0x14(%ebp), %eax
    pop     (%eax)
    mov     0x18(%ebp), %eax
    pop     (%eax)
    mov     0x1c(%ebp), %eax
    pop     (%eax)
    # Restore %ebx.
    pop     %ebx
    pop     %ebp
    ret
    
