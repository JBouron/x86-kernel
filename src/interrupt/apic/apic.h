#ifndef _APIC_H
#define _APIC_H
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

// Signal the end of interrupt handling. This must be done to signal the APIC to
// send subsequent interrupts.
void
apic_eoi(void);
#endif
