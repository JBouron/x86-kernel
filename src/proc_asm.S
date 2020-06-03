#include <macro.h>

//void do_far_return_to_proc(struct proc * const proc);
ASM_FUNC_DEF(do_far_return_to_proc):
    // To return to the execution of a process, we simply need do to an iret.
    // However, depending on the process, we are doing thing differently.
    // If the process is a ring 3 process, we need to create an iret frame on
    // the kernel stack with the following:
    //      SS
    //      ESP
    //      EFLAGS
    //      CS
    //      EIP
    // Additionally, we need to restore the segment registers to use user
    // segments.
    //
    // If the process is a ring 0 process (aka. kernel process), then the iret
    // frame should be constructed on the process' stack, with the following:
    //      EFLAGS
    //      CS
    //      EIP
    // This is because there won't be a privilege level change. Restoring the
    // segment registers can be skipped in this case, as kernel processes use
    // kernel segments which are already loaded on the cpu at this point.

    // EAX = pointer to the struct proc.
    mov     0x4(%esp), %eax

    // EBX = pointer to struct register_save_area.
    mov     %eax, %ebx
    add     REGISTERS_SAVE_OFFSET, %ebx

    // Check if this is a kernel process.
    // DL == proc->is_kernel_proc.
    mov     %eax, %edx
    add     IS_KPROC_OFFSET, %edx 
    mov     (%edx), %dl

    test    $1, %dl
    jz      prepare_iret_frame
    // This is a kernel process, set the current ESP to the process' ESP so that
    // we can build the iret frame in the process stack.
    mov     %ebx, %ecx
    add     ESP_OFF, %ecx
    mov     (%ecx), %esp
    jmp     prepare_iret_frame_skip_ss_esp

prepare_iret_frame:
    // Prepare the stack to contain the content used by iret to return to ring
    // 3, that is:
    //      SS
    //      ESP
    //      EFLAGS
    //      CS
    //      EIP
    // This part will be skipped for kernel procs.
    mov     %ebx, %ecx
    add     SS_OFF, %ecx
    pushl   (%ecx)
    mov     %ebx, %ecx
    add     ESP_OFF, %ecx
    pushl   (%ecx)
prepare_iret_frame_skip_ss_esp:
    mov     %ebx, %ecx
    add     EFLAGS_OFF, %ecx
    pushl   (%ecx)
    mov     %ebx, %ecx
    add     CS_OFF, %ecx
    pushl   (%ecx)
    mov     %ebx, %ecx
    add     EIP_OFF, %ecx
    pushl   (%ecx)

    test    $1, %dl
    // Skip restore of segment registers for kernel procs.
    jnz     restore_general_purpose_regs

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

restore_general_purpose_regs:
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
