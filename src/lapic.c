#include <lapic.h>
#include <paging.h>
#include <ioapic.h>

// lapic_def.c contains the definition of the struct lapic_t.
#include <lapic_def.c>

// This is the LAPIC address for the current CPU. It is initialized in
// init_lapic.
static volatile struct lapic_t *LAPIC;

// The IA32_APIC_BASE_MSR gives out information about the LAPIC, including its
// base address.
static uint32_t const IA32_APIC_BASE_MSR = 0x1B;

// The frequency of the LAPIC timer in Hz.
static uint64_t LAPIC_TIMER_FREQ = 0;

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
    interrupt_register_callback(vector, calibrate_timer_pit_handler);

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
    interrupt_delete_callback(vector);

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
    uint32_t const count = msec * LAPIC_TIMER_FREQ / 1000;

    // Register the callback for the given vector.
    interrupt_register_callback(vector, callback);

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

#include <lapic.test>
