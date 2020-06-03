#include <sched.h>
#include <interrupt.h>
#include <debug.h>
#include <memory.h>
#include <acpi.h>
#include <lapic.h>

// Has the scheduler been initialized ?
static bool SCHED_RUNNING = false;

// The interrupt vector to use for scheduler ticks.
#define SCHED_TICK_VECTOR   34

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
    ASSERT(!SCHED_RUNNING);
    uint8_t const ncpus = acpi_get_number_cpus();
    for (uint8_t cpu = 0; cpu < ncpus; ++cpu) {
        struct proc * const idle = create_kproc(do_idle, NULL);
        cpu_var(idle_proc, cpu) = idle;
        cpu_var(curr_proc, cpu) = idle;
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
    lapic_start_timer(2000, true, SCHED_TICK_VECTOR, sched_tick);
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
    return this_cpu_var(idle_proc);
}

void sched_run_next_proc(void) {
    struct proc * const next = pick_next_proc();
    this_cpu_var(curr_proc) = next;
    switch_to_proc(next);
}
