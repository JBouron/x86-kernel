#include <lapic.h>
#include <paging.h>
#include <ioapic.h>
#include <kernel_map.h>
#include <memory.h>
#include <interrupt.h>

// lapic_def.c contains the definition of the struct lapic_t.
#include <lapic_def.c>

// This is the LAPIC address for the current CPU. It is initialized in
// init_lapic.
static volatile struct lapic_t *LAPIC;

// The IA32_APIC_BASE_MSR gives out information about the LAPIC, including its
// base address.
static uint32_t const IA32_APIC_BASE_MSR = 0x1B;

// The frequency of the LAPIC timer in Hz. We assume that the frequency is
// constant AND the same on all cpus in the system.
static uint64_t LAPIC_TIMER_FREQ = 0;

// Get the base address of the LAPIC.
// @return: Base address of the LAPIC.
static void *get_lapic_base_addr(void) {
    uint64_t const apic_base_msr = read_msr(IA32_APIC_BASE_MSR);
    return (void*)((uint32_t)(apic_base_msr & 0xFFFFF000UL));
}


// Enable the Local APIC for the current CPU.
static void enable_apic(void) {
    // Writing the enable bit in the IA32_APIC_BASE_MSR (0x1B) only works for
    // the BSP but not the APs.
    // A more reliable option is to set the bit 8 of the Spuriouse Interrupt
    // Vector register.
    LAPIC->spurious_interrupt_vector.val |= (1 << 8);
}

// Start the LAPIC timer.
// @param count: The initial count to set up the timer to.
// @param periodic: If true, the timer is periodic firing interrupt every
// 'count' cycles. If false the timer is set to one-shot mode.
// @param vector: The vector to use to fire interrupts.
// @param masked: If true, the interrupt for the timer is masked and therefore
// no interrupt will be fired. If false the timer's interrupts will not be
// masked.
static void start_timer(uint32_t const count,
                        bool const periodic,
                        uint32_t const vector,
                        bool const masked) {
    uint32_t const periodic_bit = periodic ? (1 << 17) : 0;
    uint32_t const masked_bit = masked << 16;
    LAPIC->lvt_timer.val = masked_bit | periodic_bit | vector;
    // Note: We need to be careful here. The lvt_timer register should be
    // configured _before_ the initial_count. Enforce this order by using a
    // memory fence.
    cpu_mfence();
    LAPIC->initial_count.val = count;
    // Writing in the initial_count register starts off the timer.
}

// LAPIC timer calibration related stuff.

// The number of PIT underflows remaining.
static uint32_t num_underflows_remaining = 50;
// Set to true on the last PIT underflow.
static volatile bool calibrate_done = false;
// The value of the LAPIC timer at the time of the last PIT underflow.
static volatile uint32_t current_at_pit_interrupt = 0;

// The handler to call on PIT underflow.
static void calibrate_timer_pit_handler(
    __attribute__((unused)) struct interrupt_frame_t const * const frame) {
    num_underflows_remaining--;
    if (!num_underflows_remaining) {
        // This is the last underflow. Read out the LAPIC timer current value
        // and set the calibrate_done to true.
        current_at_pit_interrupt = LAPIC->current_count.val;
        calibrate_done = true;
        // Disable the redirection for the PIT irq so we don't receive anymore
        // of them.
        remove_redirection_for_isa_interrupt(0);
        LOG("PIT interrupt done\n");
    }
}

// The calibration function.
void calibrate_timer(void) {
    // To calibrate the LAPIC timer, we will use the legacy PIT through the IO
    // APIC.
    // The calibration methodology is as follow:
    // 1. Start the LAPIC timer with no divider and the maximum count possible.
    // 2. Setup the PIT in rate generator mode.
    // 3. Right before the PIT is started, read the current count of the LAPIC
    // timer.
    // 4. Start the PIT and let it underflow N times.
    // 5. After N overflow record the current count of the LAPIC timer and
    // disable the timers.
    // 6. Compute the frequency of the LAPIC depending on the frequency of the
    // PIT and the delta between the count of the LAPIC timer when starting and
    // ending the PIT.

    LOG("Calibrating LAPIC timer frequency\n");

    uint32_t const N = 20;
    num_underflows_remaining = N;

    // Use the 32 vector as the redirected interrupt vector for the PIT.
    uint8_t const vector = 32;

    // Start a periodic timer with the biggest count possible. Note that we
    // start the timer with the timer interrupt masked. That way the current
    // count is decremented at lapic freq without firing off interrupts.
    start_timer(0xFFFFFFFF, true, vector, true);

    // Redirect the PIT irq to the vector above and register the handler.
    redirect_isa_interrupt(0, vector);
    interrupt_register_local_callback(vector, calibrate_timer_pit_handler);

    // Setup the PIT to use a rate generator (periodic mode) with a counter of
    // 16 bits.
    uint8_t const pit_cmd_port = 0x43;
    uint8_t const pit_counter_port = 0x40;
    // Rate generator mode + 16-bit counter.
    uint8_t const cmd = (3 << 4) | (2 << 1);
    cpu_outb(pit_cmd_port, cmd);

    // To avoid numerical errors, we use a counter that is a divisor of the PIT
    // base frequency.
    uint64_t const pit_base_freq = 1193182;
    uint16_t const counter = 29102;
    ASSERT(!(pit_base_freq % counter));

    // Setup the counter for the PIT. This is done with two writes to the
    // pit_counter_port. The first sets the low byte of the counter while the
    // second sets the high byte. On the second write the PIT is started.
    cpu_outb(pit_counter_port, (uint8_t)(counter & 0xFF));
    // Enable interrupts and read the start current value of the LAPIC timer
    // right before the PIT starts (aka. writting the high byte of the counter).
    cpu_set_interrupt_flag(true);
    uint32_t const current_at_start = LAPIC->current_count.val;
    // Write the high byte of the counter and start the counter.
    cpu_outb(pit_counter_port, (uint8_t)(counter >> 8));

    // Wait until the calibration is done.
    while (!calibrate_done);
    ASSERT(current_at_pit_interrupt < current_at_start);

    cpu_set_interrupt_flag(false);

    // Delete the callback for the PIT.
    interrupt_delete_local_callback(vector);

    // Disable the LAPIC timer. This is not strictly necessary since it is
    // masked anyway. But it is better to leave it in a clean state.
    lapic_stop_timer();

    // Derive the LAPIC timer's frequency using the following formulae:
    //     Lfreq = delta * PITfreq / N
    // Where PITfreq is given by:
    //     PITfreq = PITbasefreq / counter.
    // With PITbasefreq = 1.193182 MHz.
    uint64_t const delta = current_at_start - current_at_pit_interrupt;
    uint64_t const pit_curr_freq = pit_base_freq / (uint64_t)counter;
    LAPIC_TIMER_FREQ = (delta * pit_curr_freq) / N;

    LOG("LAPIC freq = %U Hz\n", LAPIC_TIMER_FREQ);
}

void init_lapic(void) {
    LAPIC = (struct lapic_t *)get_lapic_base_addr();
    // Identity maps the LAPIC registers.
    uint32_t const flags = VM_WRITE | VM_WRITE_THROUGH | VM_CACHE_DISABLE;
    paging_map((void*)LAPIC, (void*)LAPIC, sizeof(*LAPIC), flags);
    enable_apic();
}

void ap_init_lapic(void) {
    // The LAPIC registers have already been mapped by the BSP at boot,
    // therefore all is left to do is enabling the LAPIC on this AP.
    enable_apic();
}

void lapic_eoi(void) {
    // A write of 0 in the EOI register indicates the end of interrupt.
    LAPIC->eoi.val = 0;
}

void lapic_start_timer(uint32_t const msec,
                       bool const periodic,
                       uint8_t const vector,
                       int_callback_t const callback) {
    // Since we are inserting a new callback make sure that the interrupts are
    // disabled.
    cpu_set_interrupt_flag(false);

    // Get the counter value from milliseconds.
    uint32_t const count = msec * (LAPIC_TIMER_FREQ / 1000);

    // Register the callback for the given vector.
    interrupt_register_local_callback(vector, callback);

    // Enable the interrupts again.
    cpu_set_interrupt_flag(true);

    // Start the timer without masking it.
    start_timer(count, periodic, vector, false);
}

void lapic_stop_timer(void) {
    // Mask the interrupts from the LAPIC timer.
    LAPIC->lvt_timer.val = LAPIC->lvt_timer.val | ((uint32_t)(1 << 16));
    // Writing 0 in the initial_count register stops the timer in both one-shot
    // and periodic modes.
    LAPIC->initial_count.val = 0;
}

void lapic_sleep(uint32_t const msec) {
    uint32_t const count = msec * LAPIC_TIMER_FREQ / 1000;

    // Start a one-shot timer for `msec` milliseconds. We do not want interrupts
    // here, the cpu will simply wait for the current count of the LAPIC timer
    // to be 0.
    start_timer(count, false, 0, true);

    while(LAPIC->current_count.val) {
        cpu_pause();
    }
}

// Test the validity of a configuration for the ICR.
// @param icr: The configuration to test.
// @return: true if the configuration described by `icr` param is valid, false
// otherwise.
static bool icr_is_valid(struct icr_t const * const icr) {
    // Intel's manual contain a set of rules to follow when programming the
    // interrupt command register. Those rules can be found in chapter 10.6.1.
    if (icr->delivery_mode == SYSTEM_MANAGEMENT_INTERRUPT ||
        icr->delivery_mode == INIT) {
        if (icr->vector) {
            // For SMI and INIT IPIs the vector must be 0 for future
            // compatibility.
            return false;
        }
    }
    if (icr->delivery_mode != INIT) {
        if (!icr->level) {
            // For the INIT level de-assert delivery mode this flag must be set
            // to 0; for all other delivery modes it must be set to 1.
            return false;
        }
    } else {
        // INIT delivery can take both level = 0 and level = 1. However this
        // changes the behaviour in the following way:
        // - level = 0: This send a synchronization message to all the local
        // APICs to set their arbitration IDs to the value of their APIC IDs.
        // For this mode the level must be 0 and trigger 1 (LEVEL).
        // - level = 1: This sends an INIT IPI.
        //
        // FIXME: Should we send the arbitration ID sync message in this kernel?
        // This seems to be working fine without.
        if (icr->level) {
            if (icr->vector) {
                return false;
            }
        } else if (icr->trigger_mode != LEVEL) {
            return false;
        }
    }
    return true;
}

// Write a configuration into the ICR. This sends the IPI as described by the
// value passed as parameter.
// @param icr: The configuration to write.
// Note: This function will wait until the IPI has been sent.
static void write_icr(struct icr_t const * const icr) {
    // Make sure the ICR is valid before writing it into the LAPIC.
    ASSERT(icr_is_valid(icr));

    // The interrupt command register must be written MSBytes first.
    LAPIC->interrupt_command.high = icr->high;
    LAPIC->interrupt_command.low = icr->low;

    // Wait for the IPI to be sent.
    while (LAPIC->interrupt_command.delivery_status) {
        cpu_pause();
    }
}

void lapic_send_broadcast_init(void) {
    struct icr_t icr;
    memzero(&icr, sizeof(icr));

    // Prepare an INIT IPI asserted. Vector must be 0 and the level must be 1
    // (otherwise this would send an arbitration ID sync message to the local
    // apics). We will broadcast this IPI to all the processors on the system
    // (to wake them up all at once) excepted the current processor, therefore
    // we can omit all fields related to destination.
    icr.delivery_mode = INIT;
    icr.vector = 0x0;
    icr.level = 1;
    icr.destination_shorthand = ALL_EXCL_SELF;

    write_icr(&icr);
}

void lapic_send_broadcast_sipi(void const * const trampoline) {
    struct icr_t icr;
    memzero(&icr, sizeof(icr));

    // Prepare an SIPI (aka StartUp IPI) to start the APs in a specific entry
    // point.

    // The vector of an SIPI is the frame address containing the entry point.
    // Therefore the entry point needs to start on the boundary of a physical
    // frame.
    ASSERT(is_4kib_aligned(trampoline));
    icr.vector = (uint8_t)((uint32_t)trampoline >> 12);

    // With a STARTUP delivery mode the level field must be 1.
    icr.delivery_mode = STARTUP;
    icr.level = 1;

    // Send the SIPI to all the APs at once. Hence we can omit any other
    // destination related fields.
    icr.destination_shorthand = ALL_EXCL_SELF;

    write_icr(&icr);
}

#include <lapic.test>
