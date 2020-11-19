#include <sched.h>
#include <interrupt.h>
#include <debug.h>
#include <memory.h>
#include <acpi.h>
#include <lapic.h>
#include <list.h>

// The core logic of scheduling. This file defines the functions declared in
// sched.h.

// The scheduler implementation to use.
static struct sched *SCHEDULER = NULL;

// Indicate if the scheduler is running on the current cpu.
DECLARE_PER_CPU(bool, sched_running) = false;

// Indicate if a resched is necessary on a given cpu.
DECLARE_PER_CPU(bool, resched_flag) = false;

// The interrupt vector to use for scheduler ticks.
#define SCHED_TICK_VECTOR   34

// The period between two scheduler ticks in ms.
#define SCHED_TICK_PERIOD   4

// Each cpu has an idle kernel process which only goal is to put the current cpu
// in idle. This process is run any time there is no other process to run on a
// cpu.
DECLARE_PER_CPU(struct proc *, idle_proc);

// The actual idle_proc.
static void do_idle(void * unused) {
    while (true) {
        cpu_set_interrupt_flag_and_halt();
    }
}

void sched_init(void) {
    // Initialize generic scheduling state.
    uint8_t const ncpus = acpi_get_number_cpus();
    for (uint8_t cpu = 0; cpu < ncpus; ++cpu) {
        struct proc * const idle = create_kproc(do_idle, NULL);
        if (!idle) {
            PANIC("Cannot create idle proc for cpu %u", cpu);
        }
        cpu_var(idle_proc, cpu) = idle;

        // Set the curr proc for each cpu to NULL and not their corresponding
        // idle proc. This is because upon the first context switch, if
        // curr_proc is idle, then _schedule() will wrongly think that it is
        // using idle's kernel stack.
        cpu_var(curr_proc, cpu) = NULL;
        LOG("[%u] Idle proc for %u = %p\n", cpu_id(), cpu, idle);
    }

    // Initialize the actual scheduler.
    extern struct sched ts_sched;
    SCHEDULER = &ts_sched;
    SCHEDULER->sched_init();
}

bool sched_running_on_cpu(void) {
    return this_cpu_var(sched_running);
}

// Handle a tick of the scheduler timer.
// @param frame: Unused, but mandatory to be used as an interrupt callback.
static void sched_tick(struct interrupt_frame const * const frame) {
    ASSERT(SCHEDULER);
    uint8_t const this_cpu = cpu_id();
    SCHEDULER->tick(this_cpu);
}

// Arm the LAPIC timer to send a tick in SCHED_TICK_PERIOD ms.
static void enable_sched_tick(void) {
    ASSERT(!interrupts_enabled());

    // Start the scheduler timer to fire every SCHED_TICK_PERIOD ms.
    // This will enable interrupts.
    lapic_start_timer(SCHED_TICK_PERIOD, true, SCHED_TICK_VECTOR, sched_tick);
    ASSERT(!interrupts_enabled());
}

void sched_start(void) {
    cpu_set_interrupt_flag(false);

    this_cpu_var(sched_running) = true;

    enable_sched_tick();
    // Start running the first process we can.
    schedule();

    // Initial schedule() does not return. This is because at the very least the
    // idle proc will be run.
    __UNREACHABLE__;
}

void sched_enqueue_proc(struct proc * const proc) {
    ASSERT(SCHEDULER);
    ASSERT(proc_is_runnable(proc));

    uint8_t const cpu = SCHEDULER->select_cpu_for_proc(proc);
    SCHEDULER->enqueue_proc(cpu, proc);
    proc->cpu = cpu;

    // Enqueuing triggers a resched. TODO: Implement something similar to
    // check_preempt_curr() from Linux.
    cpu_var(resched_flag, cpu) = true;
}

void sched_dequeue_proc(struct proc * const proc) {
    ASSERT(SCHEDULER);
    SCHEDULER->dequeue_proc(proc->cpu, proc);
}

void sched_update_curr(void) {
    ASSERT(SCHEDULER);

    // sched_update_curr() is called after an interrupt has been serviced, hence
    // interrupts are assumed to be disabled. See comment at the end of
    // generic_interrupt_handler().
    ASSERT(!interrupts_enabled());

    struct proc * const curr = this_cpu_var(curr_proc);
    if (proc_is_dead(curr)) {
        // This process just became dead, need to resched.
        sched_resched();
    } else {
        SCHEDULER->update_curr(cpu_id());
    }
}

void schedule(void) {
    ASSERT(SCHEDULER);

    // schedule() is called after an interrupt has been serviced, hence
    // interrupts are assumed to be disabled. See comment at the end of
    // generic_interrupt_handler().
    ASSERT(!interrupts_enabled());

    uint8_t const this_cpu = cpu_id();
    struct proc * const curr = this_cpu_var(curr_proc);
    struct proc * next = NULL;

    // We need a reschedule in the following situations:
    //  - If this is the first time schedule() is called (curr == NULL).
    //  - If an explicit resched was requested, do it now.
    //  - If this cpu was idle, check for a new proc anyway. This avoids bugs in
    //    which a cpu has processes waiting in its runqueue but never wakes up.
    //  - If the current process exited (is dead).
    bool const need_resched = !curr ||
                              cpu_is_idle(this_cpu) ||
                              cpu_var(resched_flag, this_cpu) ||
                              proc_is_dead(curr);

    if (need_resched) {
        struct proc * const prev = curr;
        struct proc * const idle = this_cpu_var(idle_proc);

        if (curr) {
            if (prev != idle && !proc_is_dead(curr)) {
                // Notify the scheduler of the context switch. This must be done
                // before picking the next proc, in case there is a single proc on
                // the system.
                SCHEDULER->put_prev_proc(this_cpu, prev);
            }
        }

        // Pick a new process to run. Default to idle_proc.
        next = SCHEDULER->pick_next_proc(this_cpu);
        if (next == NO_PROC) {
            next = idle;
        }
        ASSERT(proc_is_runnable(next));

        this_cpu_var(resched_flag) = false;

        if (next != curr) {
            // Interrupts are still disabled and will only be enabled by the
            // context switch.
            switch_to_proc(next);
        }
    }
}

void sched_resched(void) {
    this_cpu_var(resched_flag) = true;
}

bool cpu_is_idle(uint8_t const cpu) {
    struct proc * const curr = cpu_var(curr_proc, cpu);
    struct proc * const idle = cpu_var(idle_proc, cpu);

    // There is a small race condition here if the target cpu is in the middle
    // of a context switch, but this is ok.
    return curr == idle;
}

#include <sched.test>
