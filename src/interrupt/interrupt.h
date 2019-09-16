// Top level header file for interrupt related operations.
#pragma once
#include <utils/types.h>
#include <interrupt/handlers.h>

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

// Initialize interrupts on the system.
void
interrupt_init(void);

// This is the type of an interrupt handler.
typedef void (*interrupt_handler_t)(struct interrupt_desc_t const * const);

// Register an interrupt handler for a specific vector.
void
interrupt_register_vec(uint8_t const vector, interrupt_handler_t const handler);

// Register an interrupt handler for an external interrupt. Since the interrupt
// is external, it needs to be redirected to an internal interrupt vector.
void
interrupt_register_ext_vec(uint8_t const ext_vector,
                           uint8_t const redir_vector,
                           interrupt_handler_t const handler);

// Enable interrupts.
void
interrupt_enable(void);

// Disable interrupts.
void
interrupt_disable(void);
