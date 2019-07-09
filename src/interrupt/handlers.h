// Prototypes of the handlers defined in handlers.S
#ifndef _HANDLERS_H
#define _HANDLERS_H

// Generate the declaration of the interrupt handler for vector `irq`.
// The handler is named interrupt_handler_<irq>
#define DECL_HANDLER(irq) \
    void interrupt_handler_##irq(void);

// Declare all interrupt handlers required by exceptions, that is vectors 0
// through 20.
DECL_HANDLER(0)
DECL_HANDLER(1)
DECL_HANDLER(2)
DECL_HANDLER(3)
DECL_HANDLER(4)
DECL_HANDLER(5)
DECL_HANDLER(6)
DECL_HANDLER(7)
DECL_HANDLER(8)
DECL_HANDLER(9)
DECL_HANDLER(10)
DECL_HANDLER(11)
DECL_HANDLER(12)
DECL_HANDLER(13)
DECL_HANDLER(14)
DECL_HANDLER(15)
DECL_HANDLER(16)
DECL_HANDLER(17)
DECL_HANDLER(18)
DECL_HANDLER(19)
DECL_HANDLER(20)

DECL_HANDLER(33)
#undef DECL_HANDLER
#endif
