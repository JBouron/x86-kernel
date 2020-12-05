#include <macro.h>
.intel_syntax   noprefix

//void initial_ret_to_proc(struct proc * const proc);
ASM_FUNC_DEF(initial_ret_to_proc):
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
    mov     eax, [esp + 0x4]

    // EBX = pointer to struct register_save_area.
    mov     ebx, eax
    add     ebx, [REGISTERS_SAVE_OFFSET]
    mov     ebx, [ebx]

    // Check if this is a kernel process.
    // DL == proc->is_kernel_proc.
    mov     edx, eax
    add     edx, [IS_KPROC_OFFSET]
    mov     dl, [edx]

    test    dl, 1
    jz      prepare_iret_frame
    // This is a kernel process, set the current ESP to the process' ESP so that
    // we can build the iret frame in the process stack.
    mov     ecx, ebx
    add     ecx, [ESP_OFF]
    mov     esp, [ecx]
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
    mov     ecx, ebx
    add     ecx, [SS_OFF]
    push    [ecx]
    mov     ecx, ebx
    add     ecx, [ESP_OFF]
    push    [ecx]
prepare_iret_frame_skip_ss_esp:
    mov     ecx, ebx
    add     ecx, [EFLAGS_OFF]
    push    [ecx]
    mov     ecx, ebx
    add     ecx, [CS_OFF]
    push    [ecx]
    mov     ecx, ebx
    add     ecx, [EIP_OFF]
    push    [ecx]

    test    dl, 1
    // Skip restore of segment registers for kernel procs.
    jnz     restore_general_purpose_regs

    // We can now restore the register values.
    // Start with the segment registers. CS and SS are taken care of by the
    // iret. DS needs to be changed last to avoid a #GP.
    mov     ecx, ebx
    add     ecx, [GS_OFF]
    mov     ecx, [ecx]
    mov     gs, cx
    // No access to percpu variables under this line.

    mov     ecx, ebx
    add     ecx, [FS_OFF]
    mov     ecx, [ecx]
    mov     fs, cx

    mov     ecx, ebx
    add     ecx, [ES_OFF]
    mov     ecx, [ecx]
    mov     es, cx

restore_general_purpose_regs:
    // Copy the saved general purpose register values onto the stack so that we
    // can use a popa to restore them all at once.
    // The general purpose registers are in the first 32 bytes of the struct
    // register_save_area.
    mov     ecx, 0x20
    // We need to copy in reverse to make sure the order stays the same in the
    // stack.
    std
    // Make space on the stack to contain the saved registers.
    sub     esp, ecx
    // ESI = Source for copy, since we are doing decrement, ESI should point to
    // the end of the struct proc_regs.
    mov     esi, ebx
    add     esi, ecx
    sub     esi, 4
    // EDI = Destination for copy, because we are doing decrements EDI should
    // point to the top of the area that will contain the data.
    mov     edi, esp
    add     edi, ecx
    sub     edi, 4

    mov     ecx, 8
    rep     movsd

    // The top of the stack now contains the general purpose reg values. We are
    // not going to use DS anymore we can now restore it.
    mov     ecx, ebx
    add     ecx, [DS_OFF]
    mov     ecx, [ecx]
    mov     ds, cx
    // No access to DS under this line.

    // Restore general purpose reg and iret to user-space.
    popa
    iret
