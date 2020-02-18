#pragma once
#include <types.h>

// Initialize the interrupts for the kernel.
void interrupt_init(void);

// Initialize interrupts for an Application Processor.
void ap_interrupt_init(void);

// This contain information about the interrupt that triggered the call to the
// handler/callback.
struct interrupt_frame_t {
    // The value of EFLAGS at the time of the interrupt.
    uint32_t eflags;
    // The value of CS at the time of the interrupt.
    uint32_t cs;
    // The value of EIP at the time of the interrupt.
    uint32_t eip;
    // The error code pushed onto the stack by the interrupt. If the interrupt
    // does not possess an error code this field is 0.
    uint32_t error_code;
    // The vector of the interrupt.
    uint32_t vector;
} __attribute__((packed));

// An high-level callback for an interrupt.
typedef void (*int_callback_t)(struct interrupt_frame_t const *);

// Register a callback for an interrupt vector.
// @param vector: The vector that should trigger the interrupt callback once
// "raised".
// @param callback: The callback to be called everytime an interrupt with vector
// `vector` is received by the CPU.
void interrupt_register_callback(uint8_t const vector,
                                 int_callback_t const callback);

// Remove the callback for a given vector.
// @param vector: The interrupt vector for which the callback should be removed.
void interrupt_delete_callback(uint8_t const vector);

// Execute tests related to interrupts.
void interrupt_test(void);
