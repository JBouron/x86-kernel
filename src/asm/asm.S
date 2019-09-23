#include <asm/macro.h>
ASM_FUNC_DEF(__read_msr):
    push    %ebp
    mov     %esp, %ebp
    push    %ebx
    // Mov `msr_num` into %ecx
    mov     0x8(%ebp), %ecx
    rdmsr
    // *hi = %edx
    mov     0xC(%ebp), %ebx
    mov     %edx, (%ebx)
    // *low = %eax
    mov     0x10(%ebp), %ebx
    mov     %eax, (%ebx)
    pop     %ebx
    pop     %ebp
    ret

ASM_FUNC_DEF(__write_msr):
    push    %ebp
    mov     %esp, %ebp
    // %ecx specifies the MSR number.
    mov     0x8(%ebp), %ecx
    // %edx:%eax corresponds to hi:lo.
    mov     0xC(%ebp), %edx 
    mov     0x10(%ebp), %eax 
    wrmsr
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

ASM_FUNC_DEF(interrupt_disable):
    cli
    ret

ASM_FUNC_DEF(interrupt_enable):
    sti
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

ASM_FUNC_DEF(read_eflags):
    pushf
    pop     %eax
    ret

ASM_FUNC_DEF(write_eflags):
    mov     0x4(%esp), %eax
    push    %eax
    popf
    ret

ASM_FUNC_DEF(outb):
    push    %ebp
    mov     %esp, %ebp
    mov     0x8(%ebp), %edx
    mov     0xC(%ebp), %eax
    outb    %al, %dx
    pop     %ebp
    ret

ASM_FUNC_DEF(inb):
    push    %ebp
    mov     %esp, %ebp
    mov     0x8(%ebp), %edx
    inb     %dx, %al 
    pop     %ebp
    ret

ASM_FUNC_DEF(read_cr0):
    mov     %cr0, %eax
    ret

ASM_FUNC_DEF(read_cr3):
    mov     %cr3, %eax
    ret

ASM_FUNC_DEF(__do_enable_paging):
    # Enable paging with the page directory address passed as argument but stay
    # in the lower memory.
    mov     4(%esp), %eax
    # %eax now contains the address (physical) of the page directory to be used.
    # We have now everything we need to setup paging.
    # Store page dir address.
    mov     %eax, %cr3
    # Enable paging with protection bits.
    mov     %cr0, %eax
    or      $0x80000001, %eax
    mov     %eax, %cr0
    ret

ASM_FUNC_DEF(__jump_to_higher_half):
    # Paging has been enabled, but we were still using the identity mapping
    # until now. It is time to get into the higher-half kernel with an absolute
    # jump.
    call __get_stack_top
    mov %eax, %ebx
    mov 4(%esp), %eax
    lea (%eax), %eax

    mov %ebx, %esp
    jmp %eax
0:
    # Translate the stack pointer to higher half.
    add     $0xC0000000, %esp
    addl     $0xC0000000, (%esp)
    ret

