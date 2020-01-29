#include <lapic.h>
#include <paging.h>

// lapic_def.c contains the definition of the struct lapic_t.
#include <lapic_def.c>

// This is the LAPIC address for the current CPU. It is initialized in
// init_lapic.
static volatile struct lapic_t *LAPIC;

// The IA32_APIC_BASE_MSR gives out information about the LAPIC, including its
// base address.
static uint32_t const IA32_APIC_BASE_MSR = 0x1B;

// Get the base address of the LAPIC.
// @return: Base address of the LAPIC.
static void *get_lapic_base_addr(void) {
    uint64_t const apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    return (void*)((uint32_t)(apic_base_msr & 0xFFFFF000UL));
}


// Enable the Local APIC for the current CPU.
static void enable_apic(void) {
    uint64_t const apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    uint64_t const enabled = apic_base_msr | (1 << 11);
    write_msr(IA32_APIC_BASE_MSR, enabled);
}

void init_lapic(void) {
    LAPIC = (struct lapic_t *)get_lapic_base_addr();
    // Identity maps the LAPIC registers.
    uint32_t const flags = VM_WRITE | VM_WRITE_THROUGH | VM_CACHE_DISABLE;
    paging_map((void*)LAPIC, (void*)LAPIC, sizeof(*LAPIC), flags);
    enable_apic();
}

void lapic_eoi(void) {
    // A write of 0 in the EOI register indicates the end of interrupt.
    LAPIC->eoi.val = 0;
}

void lapic_start_timer(uint32_t const period,
                       bool const periodic,
                       uint8_t const vector,
                       int_callback_t const callback) {
    // Since we are inserting a new callback make sure that the interrupts are
    // disabled.
    cpu_set_interrupt_flag(false);

    // Register the callback for the given vector.
    interrupt_register_callback(vector, callback);

    // Enable the interrupts again.
    cpu_set_interrupt_flag(true);

    uint32_t const periodic_bit = periodic ? (1 << 17) : 0;
    LAPIC->lvt_timer.val = periodic_bit | vector;
    // Note: We need to be careful here. The lvt_timer register should be
    // configured _before_ the initial_count. Enforce this order by using a
    // memory fence.
    cpu_mfence();
    LAPIC->initial_count.val = period;
    // Writing in the initial_count register starts off the timer.
}

void lapic_stop_timer(void) {
    // Mask the interrupts from the LAPIC timer.
    LAPIC->lvt_timer.val = LAPIC->lvt_timer.val | ((uint32_t)(1 << 16));
    // Writing 0 in the initial_count register stops the timer in both one-shot
    // and periodic modes.
    LAPIC->initial_count.val = 0;
}

#include <lapic.test>
