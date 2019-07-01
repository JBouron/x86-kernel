.section .text
.global read_msr
.type read_msr, @function
read_msr:
    push    %ebp
    mov     %esp, %ebp
    // Mov `msr_num` into %ecx
    mov     0x8(%ebp), %ecx
    rdmsr
    // *hi = %edx
    mov     0xC(%ebp), %ebx
    mov     %edx, (%ebx)
    // *low = %eax
    mov     0x10(%ebp), %ebx
    mov     %eax, (%ebx)
    pop     %ebp
    ret

.global read_tsc
.type read_tsc, @function
read_tsc:
    push    %ebp
    mov     %esp, %ebp
    rdtsc
    // *hi = %edx
    mov     0x8(%ebp), %ebx
    mov     %edx, (%ebx)
    // *low = %eax
    mov     0xC(%ebp), %ebx
    mov     %eax, (%ebx)
    pop     %ebp
    ret
