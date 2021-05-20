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

// Start execution of a freshly spawned process. This function does not return,
// it is meant to be executed by the process itself upon its first context
// switch.
// @param next: The process to start execution for.
ASM_FUNC_DEF(initial_ret_from_spawn):
    // Note: This routine has been written with kernel processes in mind, that
    // is it should be able to work with kernel processes. This is to allow this
    // option in the future if needed. For now this feature is not implemented
    // fully nor tested.
    push    ebp
    mov     ebp, esp

    // We do not care about callee saved registers here as this function will
    // not return.

    // EDI = pointer on proc's registers field.
    mov     edi, [ebp + 0x8]
    add     edi, [REG_SAVE_OFFSET]

    // AX = Next context is user context.
    // CX = Next CS.
    mov     cx, [edi + 0x38]
    mov     dx, 1
    xor     ax, ax
    test    cx, 0x3
    cmovnz  ax, dx

    test    ax, ax
    jz      0f

    // The next context is a user context, push saved SS and ESP onto the stack.
    // This is not necessary to jump to kernel context.
    push    [edi + 0x34]
    push    [edi + 0x0C]

0:
    // Terminate IRET frame by pushing EFLAGS, CS and EIP.
    // Make sure the EFLAGS will enable interrupts.
    mov     ecx, [edi + 0x20]
    or      ecx, (1 << 9)
    push    ecx
    push    [edi + 0x38]
    push    [edi + 0x24]

    // Push the general purpose registers onto the stack.
    sub     esp, 0x20 
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

    // AX still = Next context is user context.
    test    ax, ax
    jz      0f
    // Restoring/changing GS is only possible for user processes. For kernel
    // processes the GS must not change and keep the current one.
    mov     gs, [edi + 0x28]

0:
    // DS must be changed last. This is because in the case of a user process,
    // reading memory (outside of stack) after setting CS would result in a page
    // fault due to permissions.
    mov     ds, [edi + 0x30]
    popa
    iret

// void do_context_switch(struct proc * curr, struct proc * next, bool irqs);
// Perform a context switch between `curr` and `next`.
// @param curr: MUST BE the curr process on the current cpu.
// @param next: The process to branch execution to.
// @param irqs: If true, re-enable interrupts just before returning.
ASM_FUNC_DEF(do_context_switch):
    push    ebp
    mov     ebp, esp

    // EAX = next
    mov     eax, [ebp + 0xC]

    // ECX = curr (will be prev after context switch).
    mov     ecx, [ebp + 0x8]
    
    // Push callee-saved registers onto stack.
    push    ebx
    push    edi
    push    esi

    // SI = Enable interrupts before return.
    mov     si, [ebp + 0x10]

    .extern KERNEL_STACK_PTR_OFFSET
    mov     edx, [KERNEL_STACK_PTR_OFFSET]

    // Save current kernel stack pointer. (Only if curr != NULL).
    test    ecx, ecx
    jz      0f
    mov     [ecx + edx], esp
0:
    // Perform actual context switch by switching to the kernel stack of the
    // next proc.
    mov     esp, [eax + edx]

    // ECX now contains the address of the previous running process. Put it back
    // into the runqueue.
    // FIXME: We have a layer violation here, the put prev should be handled by
    // the caller of this function. However doing so makes the initial return of
    // process more complex, as they would need to call it themselves.
    test    ecx, ecx
    jz      0f
    push    ecx
    call    sched_put_prev_proc
    add     esp, 4
0:

    // Re-enable preemption.
    call    preempt_enable_no_resched

    // Re-enable interrupts if requested.
    test    si, si
    jz      0f
    sti
0:

    // Restore callee-saved registers.
    pop     esi
    pop     edi
    pop     ebx

    pop     ebp
    ret
