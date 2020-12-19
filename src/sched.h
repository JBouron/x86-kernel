#pragma once
#include <proc.h>

// The scheduler. For now limited to idle process. More to come later.

// Used by a scheduler to indicate that there is no process to be run.
#define NO_PROC ((void*)-1UL)

// Each cpu has a pointer on the process it is currently running.
DECLARE_PER_CPU(struct proc *, curr_proc);

// Generic interface of a scheduler. A scheduler only needs to define the
// following functions:
// (This struct is heavily inspired from the struct sched_class of the Linux
// kernel).
struct sched {
    // Initialize the scheduler.
    void (*sched_init)(void);

    // Enqueue a process in the scheduler.
    // @parma proc: The process to be enqueued.
    void (*enqueue_proc)(struct proc * const proc);

    // Dequeue a process from the scheduler.
    // @parma proc: The process to be dequeued.
    void (*dequeue_proc)(struct proc * const proc);

    // Update statistics about the current process of the current cpu.
    void (*update_curr)(void);

    // Handle a tick from the scheduler timer on the current cpu.
    void (*tick)(void);

    // Select the next process to run on the current cpu.
    // @return: A struct proc * on the next proc to be run on the current cpu.If
    // no process is ready this function shall return NO_PROC.
    // Note: If applicable, this function can return the curr_proc.
    struct proc *(*pick_next_proc)(void);

    // Enqueue the previous proc running on the current cpu. This is called
    // after a new proc has been picked if the new proc is different from the
    // one running.
    // @param proc: The process that was running on the cpu.
    void (*put_prev_proc)(struct proc * proc);
};

// Initialize the scheduler. This must be called once.
void sched_init(void);

// Check whether or not the scheduler has been initialized and running on the
// current cpu.
// @return: true if the scheduler has been initialized, false otherwise.
bool sched_running_on_cpu(void);

// Start the scheduler. The cpu will start executing the first process it finds
// or go into idle.
// This function does __NOT__ return.
void sched_start(void);

// Enqueue a process in the scheduler. This function will choose which cpu to
// enqueue the proc on.
// @param proc: The process to be enqueued.
void sched_enqueue_proc(struct proc * const proc);

// Dequeue a process from the scheduler.
// @param proc: The process to be dequeued.
void sched_dequeue_proc(struct proc * const proc);

// Update the current process of the cpu.
void sched_update_curr(void);

// Trigger (if needed) another round of scheduling. This function will pick the
// next proc to run and run it. Eventually, the control will be given back to
// the current proc and it will return from the call.
void schedule(void);

// Signal to the scheduler that the current process should be rescheduled ASAP.
void sched_resched(void);

// Check whether or not a cpu is idle.
// @param cpu: The cpu to check.
// @return: true if the cpu is idle, false otherwise.
bool cpu_is_idle(uint8_t const cpu);

// Disable preemption of the current process running on the current cpu. This
// function can be called multiple times (even in nested interrupt) but
// preemption will only be enabled back if preempt_enable() is called the exact
// same number of times.
void preempt_disable(void);

// Try to enable preemption of the current process running on the current cpu.
// Note that this function will not necessarily enable preemption, it will only
// do so if it has been called the same number of times as preempt_disable().
void preempt_enable(void);

// Check if the current process on the current cpu is preemptible.
// @return: true if it is preemptible, false otherwise.
bool preemptible(void);

// Execute scheduling tests.
void sched_test(void);
