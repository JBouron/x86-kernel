// This file defines various interrupt handlers used by the kernel.
#include <asm/macro.h>

// The body for an handler, this does not contain the iret nor the push of the
// error code. This macro should only be used by the DEF_HANDLER and
// DEF_HANDLER_NO_ERROR_CODE macros.
#define _HANDLER_BODY(irq) ;\
        push    $irq ;\
        mov     %esp, %eax ;\
        push    %eax ;\
        call    generic_interrupt_handler ;\
        pop     %eax ;\
        pop     %eax

// Generate an interrupt handler for vector `irq`.
// This handler constructs a interrupt_desc_t struct on the stack and calls
// the generic_interrupt_handler.
#define DEF_HANDLER(irq) ;\
    ASM_FUNC_DEF(interrupt_handler_##irq): ;\
        _HANDLER_BODY(irq) ;\
        iret

// Define an interrupt handler for interrupt that do not have error codes.
// Instead the error code is 0.
// This is necessary since the generic_interrupt_handler *always* expect an
// error code in the interrupt_desc_t struct.
#define DEF_HANDLER_NO_ERROR_CODE(irq) ;\
    ASM_FUNC_DEF(interrupt_handler_##irq): ;\
        push    $0 ;\
        _HANDLER_BODY(irq) ;\
        pop     %eax ;\
        iret
// Define all interrupt handlers required by exceptions, that is vectors 0
// through 20.
DEF_HANDLER_NO_ERROR_CODE(0)
DEF_HANDLER_NO_ERROR_CODE(1)
DEF_HANDLER_NO_ERROR_CODE(2)
DEF_HANDLER_NO_ERROR_CODE(3)
DEF_HANDLER_NO_ERROR_CODE(4)
DEF_HANDLER_NO_ERROR_CODE(5)
DEF_HANDLER_NO_ERROR_CODE(6)
DEF_HANDLER_NO_ERROR_CODE(7)
DEF_HANDLER(8)
DEF_HANDLER_NO_ERROR_CODE(9)
DEF_HANDLER(10)
DEF_HANDLER(11)
DEF_HANDLER(12)
DEF_HANDLER(13)
DEF_HANDLER(14)
DEF_HANDLER_NO_ERROR_CODE(15)
DEF_HANDLER_NO_ERROR_CODE(16)
DEF_HANDLER(17)
DEF_HANDLER_NO_ERROR_CODE(18)
DEF_HANDLER_NO_ERROR_CODE(19)
DEF_HANDLER_NO_ERROR_CODE(20)

DEF_HANDLER_NO_ERROR_CODE(33)

#undef DEF_HANDLER
#undef DEF_HANDLER_NO_ERROR_CODE
