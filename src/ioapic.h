#pragma once
#include <types.h>

// Functions to interact with the IO APIC.

// Initialize the IO APIC.
void init_ioapic(void);

// Create a redirection entry from an ISA (legacy) interrupt vector to another
// interrupt vector. Essentially, every time an interrupt with `isa_vector` is
// raise it will get transformed into a interrupt of vector `new_vector`.
// @param isa_vector: The interrupt vector to redirect.
// @param new_vector: The interrupt vector to redirect `isa_vector` to.
void redirect_isa_interrupt(uint8_t const isa_vector, uint8_t const new_vector);

// Disable the redirection of an ISA vector.
// @param isa_vector: The ISA vector to remove the redirection of.
void remove_redirection_for_isa_interrupt(uint8_t const isa_vector);

// Execute IO APIC tests.
void ioapic_test(void);
