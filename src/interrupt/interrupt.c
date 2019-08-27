#include <interrupt/interrupt.h>
#include <utils/memory.h>
#include <asm/asm.h>
#include <utils/debug.h>
#include <interrupt/apic/apic.h>
#include <interrupt/apic/ioapic.h>
#include <interrupt/idt/idt.h>

// This file contains the logic of interrupt handling of this kernel. Here is a
// summary of how the interrupt are handled:
//
// IDT and bare interrupt handlers:
// ================================
// As defined by x86 reference, the IDT is a table that maps interrupt vector to
// their handlers. The entry `i` in the table contains a 64-bit entry containing
// various info such as the offset of the interrupt handler for this vector.
// Unfortunately, once the CPU jumps to the interrupt handler after receiving
// the corresponding interrupt, the handler as no information about what vector
// triggered this call. Thus it is not possible to use *one* handler for every
// interrupt vector.
// That is why we define *all* 255 interrupt handlers in handlers.h, one for
// each vector. These are "bare" interrupt handlers, that is interrupt handlers
// specific to *one* vector.
//
// Generic interrupt handler:
// ==========================
// Since most interrupts can be handled the same way, the bare interrupt
// handlers all call a generic interrupt handler with some information about the
// interrupt itself as argument.
// The information passed by the bare handler to the generic interrupt handler
// is the interrupt_desc_t structure defined in interrupt.h. This struct
// contains everything necessary to correctly handle an interrupt.
//
// Upon calling the generic handler, it will dispatch the interrupt to the
// handlers that have been registered for this interrupt.
//
//
// Therefore, the real logic to handle an interrupt is contained not in the the
// bare interrupt handler but rather in function that have been registered, and
// been dispatched by the generic interrupt handler.

void
interrupt_init(void) {
    // To initialize the whole interrupt subsystem we need to initialize the
    // IDT, APIC and IOAPIC. While doing so we must disable interrupts.
    interrupt_disable();
    // Since we are in a multicore setup and using the APIC, we choose to
    // disable the old legacy PIC.
    pic_disable();

    idt_init();
    apic_init();
    ioapic_init();
    apic_enable();
}

// This is the callback for each interrupt vector.
// TODO: Make it a linked list ?
// TODO: Move it somewhere else.
static interrupt_handler_t callback[256];

void
interrupt_register_vec(uint8_t const vector,
                       interrupt_handler_t const handler) {
    if (callback[vector]) {
        PANIC("Interrupt registered twice.");
    } else {
        callback[vector] = handler;
    }
}

void
interrupt_register_ext_vec(uint8_t const ext_vector,
                           uint8_t const redir_vector,
                           interrupt_handler_t const handler) {
    // Set the handler before setting up the redirection to avoid a race
    // condition.
    interrupt_register_vec(redir_vector, handler);
    // Setup the redirection in the IO APIC.
    ioapic_redirect_ext_int(IOAPIC_DEFAULT_PHY_ADDR, ext_vector, redir_vector);
}

// Handle the end of interrupt protocol.
static void
__interrupt_eoi(struct interrupt_desc_t const * const desc) {
    // Signal the end of interrupt to the local APIC, this can be done before
    // the iret. 
    ASSERT(desc);
    apic_eoi();
}

// The generic interrupt handler. This is where interrupt are dispatched to the
// correct call back function/handler.
void
generic_interrupt_handler(struct interrupt_desc_t const * const desc) {
    if (callback[desc->vector]) {
        callback[desc->vector](desc);
    } else {
        LOG("Interrupt: {\n");
        LOG("   vector = %x\n", desc->vector);
        LOG("   error code = %x\n", desc->error_code);
        LOG("   eflags = %x\n", desc->eflags);
        LOG("   cs = %x\n", desc->cs);
        LOG("   eip = %x\n", desc->eip);
        LOG("}\n");
        PANIC("Unexpected exception in kernel\n");
    }
    __interrupt_eoi(desc);
}
