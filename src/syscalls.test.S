#include <macro.h>

//void exec_sum_syscall(void * unused);
ASM_FUNC_DEF(exec_sum_syscall):
.global exec_sum_syscall_start
exec_sum_syscall_start:
    rdtsc
    mov     %eax, %ebx

    rdtsc
    mov     %eax, %ecx

    rdtsc
    mov     %eax, %edx

    rdtsc
    mov     %eax, %esi

    rdtsc
    mov     %eax, %edi

    rdtsc
    mov     %eax, %ebp

    mov     $0x0, %eax 
    int     $0x80

    add     %ecx, %ebx
    add     %edx, %ebx
    add     %esi, %ebx
    add     %edi, %ebx
    add     %ebp, %ebx

    mov     0x4(%esp), %ecx

    cmp     %ebx, %eax
    jne     fail
    movb    $1, (%ecx)
    jmp     loop
fail:
    movb    $0, (%ecx)
loop:
    jmp     loop
.global exec_sum_syscall_end
exec_sum_syscall_end:
