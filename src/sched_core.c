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

// Has the scheduler been initialized ?
static bool SCHED_RUNNING = false;

// The interrupt vector to use for scheduler ticks.
#define SCHED_TICK_VECTOR   34

// The period between two scheduler ticks in ms.
#define SCHED_TICK_PERIOD   1000

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
    ASSERT(!SCHED_RUNNING);
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

    // Scheduler is now running.
    SCHED_RUNNING = true;
}

bool sched_running(void) {
    return SCHED_RUNNING;
}

// Handle a tick of the scheduler timer.
// @param frame: Unused, but mandatory to be used as an interrupt callback.
static void sched_tick(struct interrupt_frame const * const frame) {
    ASSERT(SCHEDULER);
    uint8_t const this_cpu = this_cpu_var(cpu_id);
    LOG("[%u] Sched tick\n", this_cpu);
    SCHEDULER->tick(this_cpu);
}

void sched_start(void) {
    ASSERT(sched_running());

    // Start the scheduler timer to fire every SCHED_TICK_PERIOD ms.
    lapic_start_timer(SCHED_TICK_PERIOD, true, SCHED_TICK_VECTOR, sched_tick);

    // Start running the first process we can.
    sched_run_next_proc(NULL);

    // sched_run_next_proc() does not return.
    __UNREACHABLE__;
}

void sched_enqueue_proc(struct proc * const proc) {
    ASSERT(SCHEDULER);
    uint8_t const cpu = SCHEDULER->select_cpu_for_proc(proc);
    SCHEDULER->enqueue_proc(cpu, proc);
    proc->cpu = cpu;
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
    SCHEDULER->update_curr(this_cpu_var(cpu_id));
}

void sched_run_next_proc(struct register_save_area const * const regs) {
    ASSERT(SCHEDULER);

    uint8_t const this_cpu = this_cpu_var(cpu_id);
    struct proc * const prev = this_cpu_var(curr_proc);
    struct proc * const idle = this_cpu_var(idle_proc);

    if (prev != idle) {
        // Notify the scheduler of the context switch. This must be done before
        // picking the next proc, in case there is a single proc on the system.
        SCHEDULER->put_prev_proc(this_cpu, prev);
    }

    // Pick a new process to run. Default to idle_proc.
    struct proc * next = SCHEDULER->pick_next_proc(this_cpu);
    if (next == NO_PROC) {
        next = idle;
    }
    
    if (prev != next) {
        // We have a context switch, save the registers of the current process.
        // (current in this case is `prev).
        if (regs) {
            save_registers(prev, regs);
        }
    }

    this_cpu_var(curr_proc) = next;
    switch_to_proc(next);
}
