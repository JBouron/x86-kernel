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
        ASSERT(interrupts_enabled());
        cpu_halt();
    }
}

void sched_init(void) {
    // Initialize generic scheduling state.
    uint8_t const ncpus = acpi_get_number_cpus();
    for (uint8_t cpu = 0; cpu < ncpus; ++cpu) {
        struct proc * const idle = create_kproc(do_idle, NULL);
        cpu_var(idle_proc, cpu) = idle;
        cpu_var(curr_proc, cpu) = idle;
        LOG("[%u] Idle proc for %u = %p\n", this_cpu_var(cpu_id), cpu, idle);
    }

    // Initialize the actual scheduler.
    extern struct sched ts_sched;
    SCHEDULER = &ts_sched;
    SCHEDULER->sched_init();
}

bool sched_running_on_cpu(void) {
    return percpu_initialized() && this_cpu_var(sched_running);
}

// Handle a tick of the scheduler timer.
// @param frame: Unused, but mandatory to be used as an interrupt callback.
static void sched_tick(struct interrupt_frame const * const frame) {
    ASSERT(SCHEDULER);
    uint8_t const this_cpu = this_cpu_var(cpu_id);
    if (!cpu_is_idle(this_cpu)) {
        LOG("[%u] Sched tick.\n", this_cpu);
    }
    SCHEDULER->tick(this_cpu);
}

void sched_start(void) {
    // Start the scheduler timer to fire every SCHED_TICK_PERIOD ms.
    lapic_start_timer(SCHED_TICK_PERIOD, true, SCHED_TICK_VECTOR, sched_tick);

    this_cpu_var(sched_running) = true;

    // Start running the first process we can.
    sched_run_next_proc(NULL);

    // sched_run_next_proc() does not return.
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

// Save the register values in a process.
// @param proc: The process for which the register values should be save.
// @param regs: The current values of the registers for `proc`.
void save_registers(struct proc * const proc,
                    struct register_save_area const * const regs) {
    memcpy(&proc->registers_save, regs, sizeof(*regs));
}

void sched_update_curr(void) {
    ASSERT(SCHEDULER);

    struct proc * const curr = this_cpu_var(curr_proc);
    if (proc_is_dead(curr)) {
        // This process just became dead, need to resched.
        sched_resched();
    } else {
        SCHEDULER->update_curr(this_cpu_var(cpu_id));
    }
}

void sched_run_next_proc(struct register_save_area const * const regs) {
    ASSERT(SCHEDULER);

    uint8_t const this_cpu = this_cpu_var(cpu_id);
    struct proc * const curr = this_cpu_var(curr_proc);
    struct proc * next = NULL;

    // We need a reschedule in the following situations:
    //  - If an explicite resched was requested, do it now.
    //  - If this cpu was idle, check for a new proc anyway. This avoids bugs in
    //    which a cpu has processes waiting in its runqueue but never wakes up.
    //  - If the current process exited (is dead).
    bool const need_resched = cpu_is_idle(this_cpu) ||
                              cpu_var(resched_flag, this_cpu) ||
                              proc_is_dead(curr);

    if (need_resched) {
        struct proc * const prev = curr;
        struct proc * const idle = this_cpu_var(idle_proc);
        bool const curr_dead = proc_is_dead(curr);

        if (prev != idle && !curr_dead) {
            // Notify the scheduler of the context switch. This must be done
            // before picking the next proc, in case there is a single proc on
            // the system.
            SCHEDULER->put_prev_proc(this_cpu, prev);
        }

        // Pick a new process to run. Default to idle_proc.
        next = SCHEDULER->pick_next_proc(this_cpu);
        if (next == NO_PROC) {
            next = idle;
        }
        ASSERT(proc_is_runnable(next));
        
        if (regs && !curr_dead) {
            // FIXME: There is an inefficiency here. If prev == next (there is a
            // resched but only one proc is runnable) then we could skip saving
            // the registers and re-read them again in switch_to_proc().
            save_registers(prev, regs);
        }

        this_cpu_var(resched_flag) = false;
    } else {
        // No resched requested, resume execution of the current process.
        next = this_cpu_var(curr_proc);
        ASSERT(proc_is_runnable(next));

        // FIXME: Despite resuming the current process, we still need to save
        // the registers into the struct proc. This is because we are using
        // switch_to_proc to return to user space. A better solution would be to
        // return to the interrupt handler instead since it has all the
        // registers on its stack. This would require some changes to the
        // handler however.
        if (regs) {
            save_registers(next, regs);
        }
    }

    ASSERT(proc_is_runnable(next));
    switch_to_proc(next);
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
