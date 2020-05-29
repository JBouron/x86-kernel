#include <macro.h>

.extern REGISTERS_SAVE_OFFSET

//void do_far_return_to_proc(struct proc * const proc);
ASM_FUNC_DEF(do_far_return_to_proc):
    mov     0x4(%esp), %eax

    mov     ESP_OFFSET, %ecx
    add     %eax, %ecx
    push    (%ecx)

    // Copy the content of registers_save onto the stack.
    // The struct proc_reg is 32 bytes wide.
    mov     $0x20, %ecx
    // We need to copy in reverse to make sure the order stays the same in the
    // stack.
    std
    // Make space on the stack to contain the saved registers.
    sub     %ecx, %esp
    // ESI = Source for copy, since we are doing decrement, ESI should point to
    // the end of the struct proc_regs.
    mov     %eax, %esi
    add     REGISTERS_SAVE_OFFSET, %esi
    add     %ecx, %esi
    sub     $4, %esi
    // EDI = Destination for copy, because we are doing decrements EDI should
    // point to the top of the area that will contain the data.
    mov     %esp, %edi
    add     %ecx, %edi
    sub     $4, %edi

    mov     $8, %ecx
    rep     movsl

    popa

    pop     %esp
    
    iret
