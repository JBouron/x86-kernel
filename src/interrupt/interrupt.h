// Top level header file for interrupt related operations.
#ifndef _INTERRUPT_H
#define _INTERRUPT_H
#include <utils/types.h>

// Here are the type of IDT gate descriptor.
// Task: Upon receiving an interrupt, a task handle it.
// Int:  This is a regular interrupt handler where the CPU jumps to it upon
// receiving its corresponding interrupt.
// Trap: This is the same as `Int` but the interrupts are not disabled during
// the call to the handler.
// In this kernel we will mostly use the `Int` type.
#define IDT_GATE_DESC_TYPE_TASK (5)
#define IDT_GATE_DESC_TYPE_INT  (14)
#define IDT_GATE_DESC_TYPE_TRAP (15)

// Define a interrupt_gate for a given vector.
struct interrupt_gate_t {
    // The vector of the interrupt.
    uint8_t vector;
    // The offset of the handler.
    void (*offset)(void);
    // The segment selector used before calling the handler.
    uint16_t segment_selector;
};

// The interrupt_desc_t describes the information passed by the processor upon
// an interrupt such as the vector, the error_code of the interrupt (if
// applicable, else 0), the eip where the interrupt happened, the cs at the time
// of the interrupt and the eflags at the time of the interrupt.
// This structure, alongside the generic interrupt handler allows us to handle
// *all* interrupts and exception in a similar way.
struct interrupt_desc_t {
    // The vector of the interrupt.
    uint32_t vector;
    // The error code of the interrupt. If the interrupt does not have an error
    // code then it is set to 0.
    uint32_t error_code;
    // The EIP at which the interrupt fired.
    uint32_t eip;
    // The Code segment at the time of the interrupt.
    uint32_t cs;
    // The EFLAGS value at the time of the interrupt.
    uint32_t eflags;
};

// Generic interrupt handler taking the interrupt decs as argument.
// as argument. This is the main entry point for all interrupts and exceptions.
void
generic_interrupt_handler(struct interrupt_desc_t const * const desc);

// Initialize Interrupt Descriptor Table.
void
idt_init(void);

// Register an interrupt handler to the IDT.
void
idt_register_handler(struct interrupt_gate_t const * const handler);
#endif
