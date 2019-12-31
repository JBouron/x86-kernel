#include <lapic.h>

// lapic_def.c contains the definition of the struct lapic_t.
#include <lapic_def.c>

static struct lapic_t *LAPIC;
static uint32_t const IA32_APIC_BASE_MSR = 0x1B;

// Get the base address of the LAPIC.
// @return: Base address of the LAPIC.
static void *get_lapic_base_addr(void) {
    uint64_t const apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    return (void*)((uint32_t)(apic_base_msr & 0xFFFFF000UL));
}


static void enable_apic(void) {
    uint64_t const apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    uint64_t const enabled = apic_base_msr | (1 << 11);
    write_msr(IA32_APIC_BASE_MSR, enabled);
}

void init_lapic(void) {
    LAPIC = (struct lapic_t *)get_lapic_base_addr();
    enable_apic();
}

void lapic_eoi(void) {
    // A write of 0 in the EOI register indicates the end of interrupt.
    LAPIC->eoi.val = 0;
}

#include <lapic.test>
