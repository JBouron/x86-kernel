#pragma once
#include <interrupt.h>

// Initialized the Local APIC on the current CPU.
void init_lapic(void);

// Initialize the LAPIC on the current AP.
void ap_init_lapic(void);

// Indicate the End-of-Interrupt to the Local APIC of the current CPU.
void lapic_eoi(void);

// Initialize and start a timer on the Lapic of the current CPU.
// @param msec: The number of milliseconds before the LAPIC fires off an
// interrupt.
// @param periodic: If true the timer fires an interrupt periodically, every
// period.
// @param vector: The interrupt vector to use once the timer reaches 0.
// @param callback: The callback to be called every period.
void lapic_start_timer(uint32_t const msec,
                       bool const periodic,
                       uint8_t const vector,
                       int_callback_t const callback);

// Stop the current timer on the Local APIC of the current CPU.
// Note: This stops both one-shot and periodic timers.
void lapic_stop_timer(void);

// Calibrate the frequency of the LAPIC timer. This function assumes that the IO
// APIC is set up and initialized.
void calibrate_timer(void);

// "Sleep" for a fixed amount of time using the LAPIC timer.
// @param msec: The number of milliseconds before this function returns.
// Note: During "sleep" the core is simply busy waiting for the timer to reach
// 0.
void lapic_sleep(uint32_t const msec);

// Broadcast an INIT IPI to all processors except the current one.
// Note: This function cannot be called from a non-BSP processor.
void lapic_send_broadcast_init(void);

// Broadcast an StartUp IPI to all processors except the current one.
// Note: This function cannot be called from a non-BSP processor.
void lapic_send_broadcast_sipi(void const * const trampoline);

#define IPI_BROADCAST   255
// Send an Inter-Processor-Interrupt using the local APIC.
// @param dest_cpu: The ID of the cpu to send the interrupt to.
// @param vector: The vector to raise on the destination cpu. If this value is
// IPI_BROADCAST, send a broadcast to all cpus except the current cpu.
// Note: This function waits for the interrupt to be issued by the LAPIC before
// returning.
void lapic_send_ipi(uint8_t const dest_cpu, uint8_t const vector);

// Test LAPIC functionalities.
void lapic_test(void);
