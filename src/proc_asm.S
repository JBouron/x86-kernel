#include <macro.h>

//void do_far_return_to_proc(struct proc * const proc);
ASM_FUNC_DEF(do_far_return_to_proc):
    mov     0x4(%esp), %eax

    // EBX = pointer to struct register_save_area.
    mov     %eax, %ebx
    add     REGISTERS_SAVE_OFFSET, %ebx

    // Prepare the stack to contain the content used by iret to return to ring
    // 3, that is:
    //      SS
    //      ESP
    //      EFLAGS
    //      CS
    //      EIP
    mov     %ebx, %edx
    add     SS_OFF, %edx
    pushl   (%edx)
    mov     %ebx, %edx
    add     ESP_OFF, %edx
    pushl   (%edx)
    mov     %ebx, %edx
    add     EFLAGS_OFF, %edx
    pushl   (%edx)
    mov     %ebx, %edx
    add     CS_OFF, %edx
    pushl   (%edx)
    mov     %ebx, %edx
    add     EIP_OFF, %edx
    pushl   (%edx)

    // We can now restore the register values.
    // Start with the segment registers. CS and SS are taken care of by the
    // iret. DS needs to be changed last to avoid a #GP.
    mov     %ebx, %ecx
    add     GS_OFF, %ecx
    mov     (%ecx), %ecx
    mov     %cx, %gs
    // No access to percpu variables under this line.

    mov     %ebx, %ecx
    add     FS_OFF, %ecx
    mov     (%ecx), %ecx
    mov     %cx, %fs

    mov     %ebx, %ecx
    add     ES_OFF, %ecx
    mov     (%ecx), %ecx
    mov     %cx, %es

    // Copy the saved general purpose register values onto the stack so that we
    // can use a popa to restore them all at once.
    // The general purpose registers are in the first 32 bytes of the struct
    // register_save_area.
    mov     $0x20, %ecx
    // We need to copy in reverse to make sure the order stays the same in the
    // stack.
    std
    // Make space on the stack to contain the saved registers.
    sub     %ecx, %esp
    // ESI = Source for copy, since we are doing decrement, ESI should point to
    // the end of the struct proc_regs.
    mov     %ebx, %esi
    add     %ecx, %esi
    sub     $4, %esi
    // EDI = Destination for copy, because we are doing decrements EDI should
    // point to the top of the area that will contain the data.
    mov     %esp, %edi
    add     %ecx, %edi
    sub     $4, %edi

    mov     $8, %ecx
    rep     movsl

    // The top of the stack now contains the general purpose reg values. We are
    // not going to use DS anymore we can now restore it.
    mov     %ebx, %ecx
    add     DS_OFF, %ecx
    mov     (%ecx), %ecx
    mov     %cx, %ds
    // No access to DS under this line.

    // Restore general purpose reg and iret to user-space.
    popa
    iret
