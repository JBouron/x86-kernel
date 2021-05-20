#include <sched.h>
#include <interrupt.h>
#include <debug.h>
#include <memory.h>
#include <acpi.h>
#include <lapic.h>
#include <list.h>

// The core logic of scheduling. This file defines the functions declared in
// sched.h.

// The scheduler implementation to use. A value of NULL indicate that no
// scheduler is available.
static struct sched *SCHEDULER = NULL;

// Indicate if the scheduler is running on the current cpu.
DECLARE_PER_CPU(bool, sched_running) = false;

// Indicate if a resched is necessary on a given cpu.
DECLARE_PER_CPU(bool, resched_flag) = false;

// The number of context switches performed by a cpu so far.
DECLARE_PER_CPU(uint64_t, context_switches) = 0;

// Preemption is implemented in a similar fashion as the Linux kernel: each cpu
// has a counter preempt_count which indicate if the cpu (more specifically the
// process currently running on the cpu) is preemptible or not. preempt_count >
// 0 indicates that the proc is not preemptible, preempt_count == 0 indicates
// that it is preemptible. Having interrupts disabled also implies that the proc
// is _not_ preemptible.
// The preempt_count is not meant to be accessed directly, but rather using
// preempt_disable(), preempt_enable() and preemptible().
DECLARE_PER_CPU(uint32_t, preempt_count) = 0;

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
        LOG("[%u] Idle proc for %u = %p\n", cpu_id(), cpu, idle);

        // Set the curr proc for each cpu to NULL and not their corresponding
        // idle proc. This is because upon the first context switch, if
        // curr_proc is idle, then _schedule() will wrongly think that it is
        // using idle's kernel stack.
        cpu_var(curr_proc, cpu) = NULL;
        cpu_var(resched_flag, cpu) = false;
        cpu_var(sched_running, cpu) = false;
        cpu_var(context_switches, cpu) = 0;
        cpu_var(preempt_count, cpu) = 0;
    }

    // Initialize the actual scheduler.
    if (SCHEDULER)
        SCHEDULER->sched_init();
}

bool sched_running_on_cpu(void) {
    return this_cpu_var(sched_running);
}

// Handle a tick of the scheduler timer.
// @param frame: Unused, but mandatory to be used as an interrupt callback.
static void sched_tick(struct interrupt_frame const * const frame) {
    ASSERT(SCHEDULER);
    preempt_disable();
    SCHEDULER->tick();
    preempt_enable();
}

// Arm the LAPIC timer to send a tick in SCHED_TICK_PERIOD ms.
static void enable_sched_tick(void) {
    // Start the scheduler timer to fire every SCHED_TICK_PERIOD ms.
    // This will enable interrupts.
    lapic_start_timer(SCHED_TICK_PERIOD, true, SCHED_TICK_VECTOR, sched_tick);
}

void sched_start(void) {
    this_cpu_var(sched_running) = true;

    // Do this after setting the other vars to avoid issues.
    this_cpu_var(preempt_count) = 0;

    enable_sched_tick();
    cpu_set_interrupt_flag(true);

    // Start running the first process we can.
    schedule();

    // Initial schedule() does not return. This is because at the very least the
    // idle proc will be run.
    __UNREACHABLE__;
}

void sched_enqueue_proc(struct proc * const proc) {
    ASSERT(SCHEDULER);
    ASSERT(proc_is_runnable(proc));

    preempt_disable();
    SCHEDULER->enqueue_proc(proc);
    preempt_enable();
}

void sched_dequeue_proc(struct proc * const proc) {
    ASSERT(SCHEDULER);
    preempt_disable();
    SCHEDULER->dequeue_proc(proc);
    preempt_enable();
}

void sched_update_curr(void) {
    ASSERT(SCHEDULER);

    bool const int_enabled = interrupts_enabled();
    cpu_set_interrupt_flag(false);

    struct proc * const curr = get_curr_proc();
    if (!proc_is_runnable(curr)) {
        // The current proc is not runnable anymore, we should set the
        // resched_flag so that the next call to schedule() will choose another
        // proc to run.
        sched_resched();
    } else {
        SCHEDULER->update_curr();
    }

    cpu_set_interrupt_flag(int_enabled);
}

// Check if the current cpu needs a reschedule.
// @return: true if the current cpu needs a resched, false otherwise.
static bool curr_cpu_need_resched(void) {
    // We need a reschedule in the following situations:
    //  - If this is the first time schedule() is called (curr == NULL).
    //  - If an explicit resched was requested, do it now.
    //  - If this cpu was idle, check for a new proc anyway. This avoids bugs in
    //    which a cpu has processes waiting in its runqueue but never wakes up.
    //  - If the current process exited (is dead).
    struct proc * const curr = get_curr_proc();
    return !curr || cpu_is_idle(cpu_id()) || this_cpu_var(resched_flag) ||
        !proc_is_runnable(curr);
}

// Call put_prev_proc of the schedule with the given proc as argument. This
// function makes sure that put_prev_proc is not called on procs that are not
// runnable, NULL, or the idle proc of the current cpu.
// @param prev: The proc to pass to put_prev_proc.
// Note: This function is not static, nor is it public (in the .h). This is
// because its usage is strictly limited to the resume_proc_exec assembly
// routine.
void sched_put_prev_proc(struct proc * const prev) {
    preempt_disable();
    struct proc * const idle = this_cpu_var(idle_proc);
    if (SCHEDULER && prev && prev != idle && proc_is_runnable(prev)) {
        SCHEDULER->put_prev_proc(prev);
    }
    preempt_enable();
}

void schedule(void) {
    ASSERT(SCHEDULER);

    if (!preemptible()) {
        return;
    }

    // Because we are preemptible at this point, interrupts must be enabled.
    ASSERT(interrupts_enabled());

    // We are changing the state (e.g. curr proc) of the current cpu, preemption
    // must be disabled.
    preempt_disable();
    if (curr_cpu_need_resched()) {
        struct proc * const curr = get_curr_proc();
        struct proc * const idle = this_cpu_var(idle_proc);

        // Pick a new process to run. Default to idle_proc.
        struct proc * next = SCHEDULER->pick_next_proc();
        next = (next == NO_PROC) ? idle : next;
        ASSERT(proc_is_runnable(next));

        this_cpu_var(resched_flag) = false;

        if (next != curr) {
            this_cpu_var(context_switches) ++;
            switch_to_proc(next);
        } else {
            // No call to switch_to_proc to enable the preemption for us.
            preempt_enable_no_resched();
        }
    } else {
        // No call to switch_to_proc to enable the preemption for us.
        preempt_enable_no_resched();
    }

    // At the end of schedule() preemption and interrupts should have the same
    // state as before the call.
    ASSERT(interrupts_enabled());
    ASSERT(preemptible());
}

void sched_resched(void) {
    preempt_disable();
    this_cpu_var(resched_flag) = true;
    preempt_enable();
}

bool cpu_is_idle(uint8_t const cpu) {
    struct proc * const curr = cpu_var(curr_proc, cpu);
    struct proc * const idle = cpu_var(idle_proc, cpu);

    // There is a small race condition here if the target cpu is in the middle
    // of a context switch, but this is ok.
    return curr == idle;
}

void preempt_disable(void) {
    // Accessing percpu variable in a preemptible context is not safe, therefore
    // make sure to disable preemption but temporarily disabling interrupts.
    bool const int_flag = interrupts_enabled();
    cpu_set_interrupt_flag(false);

    this_cpu_var(preempt_count)++;

    // Reset interrupt flag as it was before this function.
    cpu_set_interrupt_flag(int_flag);

    // Make sure nothing crosses a call to preempt_disable().
    cpu_mfence();
}

// Enable preemption on the current cpu.
// @param resched: If true and the preempt_count of the current cpu becomes 0
// then calls schedule() before returning. Otherwise return normally.
static void do_preempt_enable(bool const resched) {
    // Make sure nothing crosses a call to preempt_enable().
    cpu_mfence();

    // Accessing percpu variable in a preemptible context is not safe, therefore
    // make sure to disable preemption but temporarily disabling interrupts.
    bool const int_flag = interrupts_enabled();
    cpu_set_interrupt_flag(false);

    ASSERT(this_cpu_var(preempt_count));
    uint32_t const count = --this_cpu_var(preempt_count);
    bool const sched_running = this_cpu_var(sched_running);

    // Reset interrupt flag as it was before this function.
    cpu_set_interrupt_flag(int_flag);

    if (resched && !count && sched_running) {
        // preempt_count == 0 indicate that the current process is preemptible.
        // It may have been non-preemptible for a while, therefore do a round of
        // scheduling.
        schedule();
    }
}

void preempt_enable(void) {
    do_preempt_enable(true);
}

void preempt_enable_no_resched(void) {
    do_preempt_enable(false);
}

// Get the value of this cpu's variable `var`. This macro will NOT CHECK THE
// PRERIQUISITES. This macro is meant to be used by the preemptible() function
// ONLY.
// @param var: The name of the variable, as declared using DECLARE_PER_CPU.
#define this_cpu_var_unsafe(var) (*(_THIS_CPU_VAR_PTR(var)))

bool preemptible(void) {
    // Accessing percpu variable in a preemptible context is not safe, therefore
    // make sure to disable preemption but temporarily disabling interrupts.
    bool const int_flag = interrupts_enabled();
    cpu_set_interrupt_flag(false);

    // This function is called by check_percpu_prerequesites() which will be
    // called by every access of the current cpu's percpu variable (through
    // this_cpu_var). Hence we cannot use this_cpu_var here or we will end in an
    // infinte loop. Instead use the ""unsafe"" variant which will skip the call
    // to check_percpu_prerequesites(). This is ok because preemption has been
    // disabled by disabling the interrupts.
    uint32_t const count = this_cpu_var_unsafe(preempt_count);

    // A process is preemptible iff:
    //  - The preempt_count is 0
    //  - Interrupts are enabled.
    bool const res = !count && int_flag;

    // Reset interrupt flag as it was before this function.
    cpu_set_interrupt_flag(int_flag);
    return res;
}

void preempt_reset(void) {
    this_cpu_var_unsafe(preempt_count) = 0;
}

#undef this_cpu_var_unsafe

struct proc *get_curr_proc(void) {
    preempt_disable();
    struct proc * const curr = this_cpu_var(curr_proc);
    preempt_enable();
    return curr;
}

void set_curr_proc(struct proc * const proc) {
    preempt_disable();
    this_cpu_var(curr_proc) = proc;
    preempt_enable();
}

#include <sched.test>
