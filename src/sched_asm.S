#include <macro.h>

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
    lea     0x4(%esp), %esi
    mov     %eax, %edi
    mov     $0x8, %ecx
    cld
    rep movsl

    // EAX = pointer onto dest per calling convention.

    // Fixup the values of ESP. This is because calling this function pushed the
    // return address onto the stack.
    mov     %eax, %ebx
    add     ESP_OFF, %ebx
    mov     %esp, %ecx
    add     $0x28, %ecx
    mov     %ecx, (%ebx)

    // Copy EFLAGS.
    mov     %eax, %ebx
    add     EFLAGS_OFF, %ebx
    mov     (%esp), %ecx
    mov     %ecx, (%ebx)

    // Copy EIP.
    mov     %eax, %ebx
    add     EIP_OFF, %ebx
    mov     0x24(%esp), %ecx
    mov     %ecx, (%ebx)

    // Copy segment registers. This is not strictly required since all processes
    // use the same segments while in the kernel. We are doing it for
    // completness.
    mov     %eax, %ebx
    add     GS_OFF, %ebx

    mov     %gs, %ecx
    mov     %ecx, (%ebx)
    add     $0x4, %ebx

    mov     %fs, %ecx
    mov     %ecx, (%ebx)
    add     $0x4, %ebx

    mov     %ds, %ecx
    mov     %ecx, (%ebx)
    add     $0x4, %ebx

    mov     %ss, %ecx
    mov     %ecx, (%ebx)
    add     $0x4, %ebx

    mov     %cs, %ecx
    mov     %ecx, (%ebx)
    add     $0x4, %ebx

    mov     %es, %ecx
    mov     %ecx, (%ebx)
    add     $0x4, %ebx

    add     $0x4, %esp
    popa
    ret

// void _restore_registers(struct register_save_area const * const regs);
// Note: This function will restore general purpose registers ONLY. ESP will be
// unchanged (just like with popa).
ASM_FUNC_DEF(_restore_registers):
    mov     0x4(%esp), %esi
    sub     $0x20, %esp
    mov     %esp, %edi
    mov     $0x8, %ecx
    cld
    rep movsl
    
    sub     $0x4, %esp
    mov     EFLAGS_OFF, %eax
    add     0x28(%esp), %eax
    mov     (%eax), %eax
    mov     %eax, (%esp)
    popf
    popa
    ret

//__attribute__((stdcall))
//void _schedule(struct proc * const prev, struct proc * const next);
ASM_FUNC_DEF(_schedule):
    // Having prev == NULL means that this is the very first process to be
    // executed. Hence there is no need to save the registers.
    mov     0x4(%esp), %eax
    test    %eax, %eax
    jz      .Lswitch_stack

    // Save all the registers. EAX contains the pointer where to save the
    // registers.
    add     KERNEL_REG_SAVE_OFFSET, %eax
    // =========================================================================
    // ESP MUST NOT BE MODIFIED BETWEEN THE CALL TO _save_registers AND THE
    // STACK SWITCH.
    // =========================================================================
    // This is why _save_registers takes its argument in EAX and not onto the
    // stack, otherwise we would need to add $4, %esp.
    call    _save_registers

.Lswitch_stack:
    // Change ESP to use the next proc's stack.
    mov     0x8(%esp), %eax
    add     KERNEL_REG_SAVE_OFFSET, %eax
    add     ESP_OFF, %eax
    mov     (%eax), %esp
    // We are now in the next proc.

    // We need to reload 0x4(%esp) here. This is because we changed stack. In
    // the current stack the struct proc* can be found at 0x4(%esp), this is
    // because the call to _schedule() that trigger the context switch from next
    // to something else, had next in the first arg.
    mov     0x4(%esp), %ebx

    // Change the TSS to use the next proc's kernel stack so that interrupts in
    // ring 3 will end up on the correct kernel stack.
    mov     %ebx, %eax
    add     KERNEL_STACK_BOTTOM_OFFSET, %eax 
    push    (%eax)
    call    change_tss_esp0
    add     $0x4, %esp

    // Restore the general purpose registers. Segment registers are untouched,
    // this is because all processes are using the same segments while in the
    // kernel.
    mov     %ebx, %eax
    add     KERNEL_REG_SAVE_OFFSET, %eax
    push    %eax
    call    _restore_registers
    add     $0x4, %esp

    // _schedule is declared with the stdcall attribute, meaning that it is the
    // responsibility of this function to pop off the arguments from the stack.
    // The reason we want to do that is because when processes are executing for
    // the first time we need to fake a _schedule() stack frame on its kernel
    // stack. Since this is a fake call, the function it will return to will not
    // pop out the arguments from the stack. Hence do it here.
    ret     $0x8
