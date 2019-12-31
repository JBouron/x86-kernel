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
    // Save all 8 regs onto the stack.
    pusha
    // Now we are safe to use any registers again. Construct the struct
    // interrupt_frame_t for the generic interrupt handler.
    // Make EAX point to the saved vector on the stack.
    lea     0x20(%esp), %eax
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
    // Get rid of the struct interrupt_frame_t and the pointer in the stack.
    add     $0x18, %esp
    // Restore the registers.
    popa
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
    mov     $0x0, %eax
    ret
