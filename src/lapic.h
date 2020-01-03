#pragma once
#include <interrupt.h>

// Initialized the Local APIC on the current CPU.
void init_lapic(void);

// Indicate the End-of-Interrupt to the Local APIC of the current CPU.
void lapic_eoi(void);

// Initialize and start a timer on the Lapic of the current CPU.
// @param period: The period (in LAPIC cycles) of the timer.
// @param periodic: If true the timer fires an interrupt periodically, every
// period.
// @param vector: The interrupt vector to use once the timer reaches 0.
// @param callback: The callback to be called every period.
void lapic_start_timer(uint32_t const period,
                       bool const periodic,
                       uint8_t const vector,
                       int_callback_t const callback);

// Stop the current timer on the Local APIC of the current CPU.
// Note: This stops both one-shot and periodic timers.
void lapic_stop_timer(void);

// Test LAPIC functionalities.
void lapic_test(void);
