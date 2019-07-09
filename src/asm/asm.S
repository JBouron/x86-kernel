#include <asm/macro.h>
ASM_FUNC_DEF(read_msr):
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

ASM_FUNC_DEF(read_tsc):
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

ASM_FUNC_DEF(interrupts_disable):
    cli
    ret

ASM_FUNC_DEF(interrupts_enable):
    sti
    ret

ASM_FUNC_DEF(load_gdt):
    push    %ebp
    mov     %esp, %ebp
    mov     0x8(%ebp), %eax
    lgdt    (%eax)
    pop     %ebp
    ret

ASM_FUNC_DEF(load_idt):
    push    %ebp
    mov     %esp, %ebp
    mov     0x8(%ebp), %eax
    lidt    (%eax)
    pop     %ebp
    ret

ASM_FUNC_DEF(write_cs):
    mov     0x4(%esp), %eax
# We must use a long jump to reload the value of $cs.
    ljmpl   $0x8, $end
end:
    ret

ASM_FUNC_DEF(write_ds):
    movw     0x4(%esp), %ds
    ret

ASM_FUNC_DEF(write_ss):
    movw     0x4(%esp), %ss
    ret

ASM_FUNC_DEF(write_es):
    movw     0x4(%esp), %es
    ret

ASM_FUNC_DEF(write_fs):
    movw     0x4(%esp), %fs
    ret

ASM_FUNC_DEF(write_gs):
    movw     0x4(%esp), %gs
    ret

ASM_FUNC_DEF(dummy_interrupt_handler):
    iret

ASM_FUNC_DEF(send_int):
    int $33
    ret

ASM_FUNC_DEF(pic_disable):
    movb     $0xff, %al
    outb     %al, $0xa1
    outb     %al, $0x21
    ret

ASM_FUNC_DEF(lock_up):
    cli
1:
    hlt
    jmp 1b
