// I/O APIC related stuff.
#ifndef _INTERRUPT_APIC_IOAPIC_H
#define _INTERRUPT_APIC_IOAPIC_H

#include <utils/types.h>

// From Intel's documentation (https://web.archive.org/web/20121002210153/http:
// //download.intel.com/design/archives/processors/pro/docs/24201606.pdf), the
// first IO APIC is located at 0xFECOOOOO. Subsequent IO APIC are assigned in 4K
// increments.
// All IO APICs are shared among all the processors, unlike local APIC.
// For now we are only using the first IO APIC, subsequent IO APIC will be used
// at a later stage when ACPI parsing will be implemented.
#define IOAPIC_DEFAULT_PHY_ADDR    ((p_addr)0xFEC00000)

// The IO APIC registers, to be added to the base address of the IO APIC.
#define IOAPIC_IOREGSEL    (0x00)
#define IOAPIC_IOWIN       (0x10)

// Some Mnemonics for registers.
#define IOAPIC_REG_IOAPICID     (0x00)
#define IOAPIC_REG_IOAPICVER    (0x01)
#define IOAPIC_REG_IOAPICARB    (0x02)
#define IOAPIC_REG_IOREDTBL(n)  (0x10 + 2*(n))

// Initialize the default IO APIC to handle external interrupts.
void
ioapic_init(void);

// Program the IO APIC to redirect external interrupts of vector `ext_vector` as
// interrupt with vector `dest_vector` to the local APIC.
void
ioapic_redirect_ext_int(v_addr const ioapic,
                        uint8_t const ext_vector,
                        uint8_t const dest_vector);
#endif
