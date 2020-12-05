#include <macro.h>
.intel_syntax   noprefix

// void _save_registers(struct register_save_area * const dest).
// NOTE: ARGUMENT IS PASSED IN EAX, __NOT__ THE STACK.
// This function is meant to be used by _schedule() only. All general purpose
// registers and segment registers will be saved in the destination. The saved
// EIP will be the return address of this function (that is the address of the
// first instruction followin the call to this function). The saved ESP will be
// the value of ESP right before executing the call instruction to this
// function.
ASM_FUNC_DEF(_save_registers):
    pusha
    pushf

    // Copy GP registers.
    lea     esi, [esp + 0x4]
    mov     edi, eax
    mov     ecx, 0x8
    cld
    rep movsd

    // EAX = pointer onto dest per calling convention.

    // Fixup the values of ESP. This is because calling this function pushed the
    // return address onto the stack.
    mov     ebx, eax
    add     ebx, [ESP_OFF]
    mov     ecx, esp
    add     ecx, 0x28
    mov     [ebx], ecx

    // Copy EFLAGS.
    mov     ebx, eax
    add     ebx, [EFLAGS_OFF]
    mov     ecx, [esp]
    mov     [ebx], ecx

    // Copy EIP.
    mov     ebx, eax
    add     ebx, [EIP_OFF]
    mov     ecx, [esp + 0x24]
    mov     [ebx], ecx

    // Copy segment registers. This is not strictly required since all processes
    // use the same segments while in the kernel. We are doing it for
    // completness.
    mov     ebx, eax
    add     ebx, [GS_OFF]

    mov     ecx, gs
    mov     [ebx], ecx
    add     ebx, 4

    mov     ecx, fs
    mov     [ebx], ecx
    add     ebx, 4

    mov     ecx, ds
    mov     [ebx], ecx
    add     ebx, 4

    mov     ecx, ss
    mov     [ebx], ecx
    add     ebx, 4

    mov     ecx, cs
    mov     [ebx], ecx
    add     ebx, 4

    mov     ecx, es
    mov     [ebx], ecx
    add     ebx, 4

    add     esp, 0x4
    popa
    ret

// void _restore_registers(struct register_save_area const * const regs);
// Note: This function will restore general purpose registers ONLY. ESP will be
// unchanged (just like with popa).
ASM_FUNC_DEF(_restore_registers):
    mov     esi, [esp + 0x4]
    sub     esp, 0x20
    mov     edi, esp
    mov     ecx, 0x8
    cld
    rep movsd
    
    sub     esp, 0x4
    mov     eax, [EFLAGS_OFF]
    add     eax, [esp + 0x28]
    mov     eax, [eax]
    mov     [esp], eax
    popf
    popa
    ret

//__attribute__((stdcall))
//void _schedule(struct proc * const prev, struct proc * const next);
ASM_FUNC_DEF(_schedule):
    // Having prev == NULL means that this is the very first process to be
    // executed. Hence there is no need to save the registers.
    mov     eax, [esp + 0x4]
    test    eax, eax
    jz      .Lswitch_stack

    // Save all the registers. EAX contains the pointer where to save the
    // registers.
    add     eax, [KERNEL_REG_SAVE_OFFSET]
    // =========================================================================
    // ESP MUST NOT BE MODIFIED BETWEEN THE CALL TO _save_registers AND THE
    // STACK SWITCH.
    // =========================================================================
    // This is why _save_registers takes its argument in EAX and not onto the
    // stack, otherwise we would need to add $4, %esp.
    call    _save_registers

.Lswitch_stack:
    // Change ESP to use the next proc's stack.
    mov     eax, [esp + 0x8]
    add     eax, [KERNEL_REG_SAVE_OFFSET]
    add     eax, [ESP_OFF]
    mov     esp, [eax]
    // We are now in the next proc.

    // We need to reload 0x4(%esp) here. This is because we changed stack. In
    // the current stack the struct proc* can be found at 0x4(%esp), this is
    // because the call to _schedule() that trigger the context switch from next
    // to something else, had next in the first arg.
    mov     ebx, [esp + 0x4]

    // Change the TSS to use the next proc's kernel stack so that interrupts in
    // ring 3 will end up on the correct kernel stack.
    mov     eax, ebx
    add     eax, [KERNEL_STACK_BOTTOM_OFFSET]
    push    [eax]
    call    change_tss_esp0
    add     esp, 0x4

    // Restore the general purpose registers. Segment registers are untouched,
    // this is because all processes are using the same segments while in the
    // kernel.
    mov     eax, ebx
    add     eax, [KERNEL_REG_SAVE_OFFSET]
    push    eax
    call    _restore_registers
    add     esp, 0x4

    // _schedule is declared with the stdcall attribute, meaning that it is the
    // responsibility of this function to pop off the arguments from the stack.
    // The reason we want to do that is because when processes are executing for
    // the first time we need to fake a _schedule() stack frame on its kernel
    // stack. Since this is a fake call, the function it will return to will not
    // pop out the arguments from the stack. Hence do it here.
    ret     0x8
