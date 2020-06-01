#pragma once
#include <types.h>

// The vector number used by syscalls.
#define SYSCALL_VECTOR  0x80

// Initialize the interrupts for the kernel.
void interrupt_init(void);

// Initialize interrupts for an Application Processor.
void ap_interrupt_init(void);

// This contain information about the interrupt that triggered the call to the
// handler/callback.
struct interrupt_frame {
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
    // A snapshot of the registers right _before_ the interrupt happened.
    struct register_save_area const *registers;
} __attribute__((packed));

// Callbacks
// =========
//      Callbacks are an easy way to dynamically dispatch functions calls when
// certain interrupts are raised. The kernel can register interrupt callback to
// specific interrupt vectors.
// There are two level of callbacks: global and cpu local. Global callbacks are
// global to all cpus, while cpu local are, well, local to the cpu.
// When an interrupt is raised, the generic interrupt handler look for a local
// callback for the given vector. If such a callback is present, the handler
// calls the callback and irets. If no local callback is registered for that
// vector, the handler looks for a global callback and, if found, calls it. If
// there is no local nor global callback registered for the vector a PANIC is
// raised.

// An high-level callback for an interrupt.
typedef void (*int_callback_t)(struct interrupt_frame const *);

// Register a global callback for an interrupt vector.
// @param vector: The vector that should trigger the interrupt callback once
// "raised".
// @param callback: The callback to be called everytime an interrupt with vector
// `vector` is received by a CPU.
void interrupt_register_global_callback(uint8_t const vector,
                                        int_callback_t const callback);

// Register a local callback for an interrupt vector.
// @param vector: The vector that should trigger the interrupt callback once
// "raised".
// @param callback: The callback to be called everytime an interrupt with vector
// `vector` is received by the current CPU.
void interrupt_register_local_callback(uint8_t const vector,
                                       int_callback_t const callback);

// Remove a global callback for a given vector.
// @param vector: The interrupt vector for which the callback should be removed.
void interrupt_delete_global_callback(uint8_t const vector);

// Remove a local callback for a given vector.
// @param vector: The interrupt vector for which the callback should be removed.
void interrupt_delete_local_callback(uint8_t const vector);

// Execute tests related to interrupts.
void interrupt_test(void);
