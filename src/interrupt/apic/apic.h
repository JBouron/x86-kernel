#pragma once
#include <utils/types.h>

// Returns wether the APIC functionality is supported by the processor.
uint8_t
is_apic_present(void);

// Enable the local APIC.
void
apic_enable(void);

// Disable the local APIC.
void
apic_disable(void);

// Initialize the local APIC.
void
apic_init(void);

// Start the periodic APIC timer and set it to fire the interrupt vector
// provided as arg.
void
apic_start_periodic_timer(uint8_t const vector);

// Signal the end of interrupt handling. This must be done to signal the APIC to
// send subsequent interrupts.
void
apic_eoi(void);
