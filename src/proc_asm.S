#include <macro.h>
.intel_syntax   noprefix

// void save_registers(struct register_save_area * const dest, void const
// * const eip);
// This function will save the current values of all the registers into the
// destination struct register_save_area.
// @param dest: Where to save the values of the registers.
// @param eip: The value to used for the saved EIP. See not below.
// Note: Two registers have a special treatment in this function:
//  - ESP: The saved value of ESP is the value just _before_ the call to
//  save_register, that is, the saved value will be a pointer onto the first
//  argument, not onto the return value of the function.
//  - EIP: The caller of the function decides what to put for the saved EIP.
ASM_FUNC_DEF(save_registers):
    push    ebp
    mov     ebp, esp

    // Callee saved.
    push    ebx
    push    esi

    // EBX = Pointer to destination.
    mov     ebx, [ebp + 0x8]

    mov     [ebx + 0x00], edi
    mov     [ebx + 0x04], esi

    // Save EBP
    mov     esi, [ebp]
    mov     [ebx + 0x08], esi

    // Save ESP: We take the ESP right before the call instruction, that is:
    lea     esi, [ebp + 0x8]
    mov     [ebx + 0x0C], esi

    // Save EBX
    mov     esi, [ebp - 0x4]
    mov     [ebx + 0x10], esi

    mov     [ebx + 0x14], edx
    mov     [ebx + 0x18], ecx
    mov     [ebx + 0x1C], eax

    // Save EFLAGS
    pushf
    pop     eax
    mov     [ebx + 0x20], eax

    // Save EIP: We take the return address of this call, that is:
    mov     eax, [ebp + 0xC]
    mov     [ebx + 0x24], eax

    // Save segment registers. Note: The mov [...], <seg reg> will only mov
    // 16bit, and MOVSX is not possible with segment registers, hence we need to
    // manually zero out the 16 MSBs of the destination.
    mov     WORD PTR [ebx + 0x28], gs
    mov     WORD PTR [ebx + 0x2A], 0x0
    mov     WORD PTR [ebx + 0x2C], fs
    mov     WORD PTR [ebx + 0x2E], 0x0
    mov     WORD PTR [ebx + 0x30], ds
    mov     WORD PTR [ebx + 0x32], 0x0
    mov     WORD PTR [ebx + 0x34], ss
    mov     WORD PTR [ebx + 0x36], 0x0
    mov     WORD PTR [ebx + 0x38], cs
    mov     WORD PTR [ebx + 0x3A], 0x0
    mov     WORD PTR [ebx + 0x3C], es
    mov     WORD PTR [ebx + 0x3E], 0x0

    pop     esi
    pop     ebx
    leave
    ret

// void resume_proc_exec(struct proc const * const next);
// This function will start/resume the execution of a process. This function
// DOES NOT RETURN.
// @param next: The process to resume/start execution.
ASM_FUNC_DEF(resume_proc_exec):
    push    ebp
    mov     ebp, esp

    // We do not care about callee saved registers here as this function will
    // not return.

    // ESI = pointer on proc.
    mov     esi, [ebp + 0x8]
    // EDI = pointer on saved registers.
    mov     edi, [REG_SAVE_OFFSET]
    add     edi, esi

    // AX = CPL.
    mov     ax, cs
    and     ax, 0x3

    // CX = Saved priv level.
    lea     ecx, [edi + 0x38]
    mov     cx, [ecx]
    and     cx, 0x3
    
    // Compare the CPL with the saved PL to know if we are about to switch to
    // user space or kernel space.
    cmp     ax, cx
    jne     0f

    // The task we are about to switch to had its execution inside the kernel at
    // the time of its last context switch. Hence we need to switch to its
    // stack to build the IRET frame.
    mov     esp, [edi + 0xC]

0:
    // AX = Next context is user context.
    mov     dx, 1
    xor     ax, ax
    xor     cx, 0x3
    cmovz   ax, dx

    test    ax, ax
    jz      0f

    // The next context is a user context, push saved SS and ESP onto the stack.
    push    [edi + 0x34]
    push    [edi + 0x0C]

0:
    // Terminate IRET frame by pushing EFLAGS, CS and EIP.
    push    [edi + 0x20]
    push    [edi + 0x38]
    push    [edi + 0x24]

    // Push the general purpose registers onto the stack.
    sub     esp, 0x20 
    // Save EDI temporarily.
    push    edi

    lea     esi, [edi + 0x1C]
    lea     edi, [esp + 0x20]
    std
    mov     ecx, 8
    rep     movsd
    pop     edi
    
    // Restore segment regsiters.
    mov     fs, [edi + 0x2C]
    mov     es, [edi + 0x3C]

    test    ax, ax
    jz      0f
    // Restoring/changing GS is only possible for user processes.
    mov     gs, [edi + 0x28]

0:
    mov     ds, [edi + 0x30]
    popa
    iret

// void do_context_switch(struct proc * curr, struct proc * next);
// Perform a context switch between `curr` and `next`.
// @param curr: MUST BE the curr process on the current cpu.
// @param next: The process to branch execution to.
ASM_FUNC_DEF(do_context_switch):
    push    ebp
    mov     ebp, esp

    // If there are no current process on the cpu (ie. this is the very first
    // process executing on the cpu, ignore the register saving and directly
    // execute the next proc.
    mov     eax, [ebp + 0x8]
    test    eax, eax
    jz      skip_reg_save

    add     eax, [REG_SAVE_OFFSET]
    
    // Save the registers of the current process into its save area (registers
    // field of its struct proc). For the EIP, use 0f so that the next time this
    // process executes it will return to the caller of this function.
    push    OFFSET FLAT : 0f
    push    eax
    call    save_registers
    add     esp, 0x8

skip_reg_save:
    // Start executing the next process.
    push    [ebp + 0xC]
    call    resume_proc_exec
    // Unreachable.
    int3

0:
    // When execution starts again at this point, the top two values on the
    // stacks are the ptr to the struct register_save_area and the saved EIP
    // (right before the call to save_registers(). Doing a LEAVE here makes sure
    // that we don't forget any pop/add esp and prevent stack frame corruption.
    leave
    ret
