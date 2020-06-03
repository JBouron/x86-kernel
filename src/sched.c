#include <sched.h>
#include <interrupt.h>
#include <debug.h>
#include <memory.h>
#include <acpi.h>
#include <lapic.h>
#include <list.h>
#include <ipm.h>

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

// Each cpu has a runqueue in which runnable processes are enqueued and waiting
// to be run. Each run queue is protected by a lock.
DECLARE_PER_CPU(struct list_node, runqueue);
DECLARE_PER_CPU(spinlock_t, runqueue_lock);

// Helper macros to get/lock the runqueue of a particular cpu or the current
// cpu.
#define cpu_runqueue(cpu)        (&cpu_var(runqueue, (cpu)))
#define this_runqueue()          (&cpu_runqueue(this_cpu_var(cpu_id)))
#define lock_cpu_runqueue(cpu)   (spinlock_lock(&cpu_var(runqueue_lock, cpu)))
#define lock_this_runqueue()     (lock_cpu_runqueue(this_cpu_var(cpu_id)))
#define unlock_cpu_runqueue(cpu) (spinlock_unlock(&cpu_var(runqueue_lock, cpu)))
#define unlock_this_runqueue()   (unlock_cpu_runqueue(this_cpu_var(cpu_id)))

// The actual idle_proc.
static void do_idle(void * unused) {
    while (true) {
        ASSERT(interrupts_enabled());
        cpu_halt();
    }
}

void sched_init(void) {
    ASSERT(!SCHED_RUNNING);
    uint8_t const ncpus = acpi_get_number_cpus();
    for (uint8_t cpu = 0; cpu < ncpus; ++cpu) {
        struct proc * const idle = create_kproc(do_idle, NULL);
        cpu_var(idle_proc, cpu) = idle;
        cpu_var(curr_proc, cpu) = idle;
        list_init(&cpu_var(runqueue, cpu));
        spinlock_init(&cpu_var(runqueue_lock, cpu));
        LOG("[%u] Idle proc for %u = %p\n", this_cpu_var(cpu_id), cpu, idle);
    }
    SCHED_RUNNING = true;
}

bool sched_running(void) {
    return SCHED_RUNNING;
}

// Handle a tick of the scheduler
// @param frame: Unused, but mandatory to be used as an interrupt callback.
static void sched_tick(struct interrupt_frame const * const frame) {
    LOG("[%u] Sched tick\n", this_cpu_var(cpu_id));
    // TODO: Update runtime stats here.
}

void sched_start(void) {
    ASSERT(sched_running());
    lapic_start_timer(SCHED_TICK_PERIOD, true, SCHED_TICK_VECTOR, sched_tick);
    sched_run_next_proc();

    // sched_run_next_proc() does not return.
    __UNREACHABLE__;
}

void sched_update_curr(struct register_save_area const * const regs) {
    struct proc * const curr = this_cpu_var(curr_proc);
    LOG("[%u] Sched update curr %p\n", this_cpu_var(cpu_id), curr);

    // Save the latest register values to the struct proc in case we schedule
    // another process after this one.
    memcpy(&curr->registers_save, regs, sizeof(*regs));
}

// Select the next process to be run on the current cpu.
// If no process is available, this function will return the idle_proc of the
// current cpu. Therefore it will _always_ return a valid process.
// @return: The next process to run.
static struct proc *pick_next_proc(void) {
    LOG("[%u] Sched pick next proc\n", this_cpu_var(cpu_id));

    struct list_node * const cpu_rq = &this_cpu_var(runqueue);

    if (!list_size(cpu_rq)) {
        return this_cpu_var(idle_proc);
    } else {
        return list_first_entry(cpu_rq, struct proc, rq);
    }
}

void sched_run_next_proc(void) {
    struct proc * const curr = this_cpu_var(curr_proc);
    struct proc * const idle = this_cpu_var(idle_proc);

    if (curr != idle) {
        enqueue_proc(this_cpu_var(curr_proc));
    }

    struct proc * const next = pick_next_proc();

    if (next != idle) {
        dequeue_proc(next);
    }

    this_cpu_var(curr_proc) = next;
    switch_to_proc(next);
}

void enqueue_proc_on(uint8_t const cpu, struct proc * const proc) {
    lock_cpu_runqueue(cpu);

    struct list_node * const cpu_rq = cpu_runqueue(cpu);
    list_add_tail(cpu_rq, &proc->rq);
    proc->cpu = cpu;

    unlock_cpu_runqueue(cpu);
}

// Check if a process is currently enqueued in a runqueue.
// @param rq: The runqueue.
// @param proc: The process we are looking for in `rq`.
// @return: true if proc is in `rq` false otherwise.
// Note: This function assumes that the lock on the runqueue is held by the
// current cpu.
static bool in_runqueue(struct list_node const * const rq,
                        struct proc const * const proc) {
    struct proc * it;
    list_for_each_entry(it, rq, rq) {
        if (it == proc) {
            return true;
        }
    }
    return false;
}

void dequeue_proc_on(uint8_t const cpu, struct proc * const proc) {
    lock_cpu_runqueue(cpu);

    struct list_node * const cpu_rq = cpu_runqueue(cpu);

    ASSERT(proc->cpu == cpu);
    ASSERT(in_runqueue(cpu_rq, proc));

    list_del(&proc->rq);
    // Reset the cpu field to 0xFF to show that this proc is not enqueue
    // anymore.
    proc->cpu = 0xFF;

    unlock_cpu_runqueue(cpu);
}
