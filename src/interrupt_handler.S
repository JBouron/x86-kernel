#include <macro.h>

#define interrupt_handler_no_err_code(vector)   ;\
    .global interrupt_handler_ ## vector        ;\
    interrupt_handler_ ## vector :              ;\
        pushl   $0x0                            ;\
        pushl   $ ## vector                     ;\
        jmp     _interrupt_handler_common

#define interrupt_handler_err_code(vector)   ;\
    .global interrupt_handler_ ## vector     ;\
    interrupt_handler_ ## vector :           ;\
        pushl   $ ## vector                  ;\
        jmp     _interrupt_handler_common

// This is not exactly a function. Interrupt handlers push their vector onto the
// stack and jump to this routine.
// This routine saves the registers, calls the generic interrupt handler, and
// later restore the registers before iret.
_interrupt_handler_common:
    // Save registers onto the stack in reverse order as they appear in struct
    // register_save_area. We cannot use general purpose registers (except ESP)
    // until the pusha.
    // Depending on the processor, pushing the segment registers might not zero
    // the top 16 bits of the dword on the stack which means we can end up with
    // garbage values. To avoid that, precede all segment reg push with a pushl
    // 0; add 4, %esp so that we are sure the top 16-bits will always be 0.
    pushl   $0
    add     $0x4, %esp
    push    %es
    // CS is already on the stack. Since we pushed %ES, the value on CS on the
    // stack is at offset 0xC.
    push    0x10(%esp)
    // The current value of %SS might not be the original value of SS when the
    // processor got interrupted if a task in ring 3 was running. We will patch
    // it up later.
    pushl   $0
    add     $0x4, %esp
    push    %ss
    pushl   $0
    add     $0x4, %esp
    push    %ds
    pushl   $0
    add     $0x4, %esp
    push    %fs
    pushl   $0
    add     $0x4, %esp
    push    %gs
    // Push the EIP, it was pused on the interrupt frame.
    push    0x20(%esp)
    // Push EFLAGS, it was pushed on the interrupt frame as well.
    push    0x2C(%esp)
    // Now push all general purpose registers.
    pusha

    // In case a privilege change happened, the saved values for the stack
    // segment register and ESP are wrong since those are the SS and ESP used by
    // the interrupt handler, _not_ the original process.
    // If that is the case, then those values can be found at the top of the
    // handler's stack frame. We therefore need the fixup the registers in the
    // save area.
    // EAX = privilege level of current code segment.
    mov     %cs, %eax
    and     $0x3, %eax 
    // EBX = privilege level of interrupted context's code segment.
    mov     56(%esp), %ebx
    and     $0x3, %ebx

    cmp     %eax, %ebx
    jne     do_fixup_priv_change
    // There was no privilege change then the saved SS is correct. However the
    // saved ESP is still incorrect as we pushed some values on the stack before
    // the pusha. We need to fix this. The actual value _before_ the interrupt
    // should be the current value of ESP + 88:
    //                                         v To point to the top before int.
    //                64 + (2 * 4) + (3 * 4) + 4 = 88 bytes.
    //       reg. save^   vec+err^     ^- eip, cs, eflags.
    mov     %esp, %eax
    add     $88, %eax
    mov     %eax, 0xC(%esp)

    jmp     do_fixup_priv_change_end

do_fixup_priv_change:
    // The original SS from the process was pushed on the stack and can be found
    // at ESP + 88, its saved location is ESP + 52.
    // The original ESP from the process can be found at ESP + 84 and saved at
    // ESP + 12.
    mov     88(%esp), %ecx
    mov     %ecx, 52(%esp)
    mov     84(%esp), %ecx
    mov     %ecx, 12(%esp)

do_fixup_priv_change_end:
    // Construct the struct interrupt_frame_t for the generic interrupt handler.
    // Push pointer to the struct register_save_area containing the snapshot of
    // registers created above.
    push    %esp
    // EAX = initial ESP after the interrupt occured, pointing to the error
    // code.
    lea     68(%esp), %eax
    // Push the vector.
    pushl   (%eax)
    // Push the error code.
    pushl   0x4(%eax)
    // Push the EIP.
    pushl   0x8(%eax)
    // Push CS.
    pushl   0xC(%eax)
    // Push EFLAGS.
    pushl   0x10(%eax)
    // Now push the address of the struct on the stack for the argument of the
    // generic interrupt handler.
    push    %esp 
    // Call the generic_interrupt_handler.
    call generic_interrupt_handler
    // The interrupt has been dealt with. We can now restore the registers and
    // return from the interrupt.
    // Get rid of the struct interrupt_frame and the pointer in the stack.
    add     $0x1C, %esp
    // Restore the registers.
    popa
    // FIXME: Get rid of the saved registers values, they are unused for now.
    add     $32, %esp
    // Now get rid of the vector AND the error code. As described in the Intel
    // manual chapter 6.13 ERROR CODE, the error code is pushed onto the stack
    // when the interrupt is triggered but it is the responsibility of the
    // handler to pop it.
    add     $0x8, %esp
    // Stack is clear, registers are restored we can now iret.
    iret

interrupt_handler_no_err_code(0)
interrupt_handler_no_err_code(1)
interrupt_handler_no_err_code(2)
interrupt_handler_no_err_code(3)
interrupt_handler_no_err_code(4)
interrupt_handler_no_err_code(5)
interrupt_handler_no_err_code(6)
interrupt_handler_no_err_code(7)
interrupt_handler_err_code(8)
interrupt_handler_no_err_code(9)
interrupt_handler_err_code(10)
interrupt_handler_err_code(11)
interrupt_handler_err_code(12)
interrupt_handler_err_code(13)
interrupt_handler_err_code(14)
interrupt_handler_no_err_code(15)
interrupt_handler_no_err_code(16)
interrupt_handler_err_code(17)
interrupt_handler_no_err_code(18)
interrupt_handler_no_err_code(19)
interrupt_handler_no_err_code(20)
interrupt_handler_no_err_code(21)
interrupt_handler_no_err_code(22)
interrupt_handler_no_err_code(23)
interrupt_handler_no_err_code(24)
interrupt_handler_no_err_code(25)
interrupt_handler_no_err_code(26)
interrupt_handler_no_err_code(27)
interrupt_handler_no_err_code(28)
interrupt_handler_no_err_code(29)
interrupt_handler_no_err_code(30)
interrupt_handler_no_err_code(31)
interrupt_handler_no_err_code(32)
interrupt_handler_no_err_code(33)

#define cond_ret_handler(vector)                                    ;\
    cmp     $ ## vector, %eax                                       ;\
    jne     not_eq_ ## vector                                       ;\
    mov     $interrupt_handler_ ## vector, %eax   ;\
    ret                                                             ;\
not_eq_ ## vector :
    
ASM_FUNC_DEF(get_interrupt_handler):
    mov     0x4(%esp), %eax
    cond_ret_handler(0)
    cond_ret_handler(1)
    cond_ret_handler(2)
    cond_ret_handler(3)
    cond_ret_handler(4)
    cond_ret_handler(5)
    cond_ret_handler(6)
    cond_ret_handler(7)
    cond_ret_handler(8)
    cond_ret_handler(9)
    cond_ret_handler(10)
    cond_ret_handler(11)
    cond_ret_handler(12)
    cond_ret_handler(13)
    cond_ret_handler(14)
    cond_ret_handler(15)
    cond_ret_handler(16)
    cond_ret_handler(17)
    cond_ret_handler(18)
    cond_ret_handler(19)
    cond_ret_handler(20)
    cond_ret_handler(21)
    cond_ret_handler(22)
    cond_ret_handler(23)
    cond_ret_handler(24)
    cond_ret_handler(25)
    cond_ret_handler(26)
    cond_ret_handler(27)
    cond_ret_handler(28)
    cond_ret_handler(29)
    cond_ret_handler(30)
    cond_ret_handler(31)
    cond_ret_handler(32)
    cond_ret_handler(33)
    mov     $0x0, %eax
    ret
