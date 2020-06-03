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

    // Select on which cpu the process should be enqueued.
    // @param proc: A non-enqueued process.
    // @return: The cpu id of the cpu on which `proc` should be enqueued.
    uint8_t (*select_cpu_for_proc)(struct proc const * const proc);

    // Enqueue a process on a cpu.
    // @param cpu: The cpu to enqueue the process on.
    // @parma proc: The process to be enqueued.
    void (*enqueue_proc)(uint8_t const cpu, struct proc * const proc);

    // Dequeue a process from a cpu.
    // @param cpu: The cpu to dequeue the process from.
    // @parma proc: The process to be dequeued.
    void (*dequeue_proc)(uint8_t const cpu, struct proc * const proc);

    // Update statistics about the current process of a cpu.
    // @param cpu: The cpu.
    void (*update_curr)(uint8_t const cpu);

    // Handle a tick from the scheduler timer.
    // @param cpu: The cpu.
    void (*tick)(uint8_t const cpu);

    // Select the next process to run.
    // @param cpu: The cpu for which a next proc should be picked.
    // @return: A struct proc * on the next proc to be run on `cpu`. If no
    // process is ready this function shall return NO_PROC.
    // Note: If applicable, this function can return the curr_proc.
    struct proc *(*pick_next_proc)(uint8_t const cpu);

    // Enqueue the previous proc running on a given cpu. This is called after a
    // new proc has been picked if the new proc is different from the one
    // running.
    // @param cpu: The cpu to put the prev proc on.
    // @param proc: The process that was running on the cpu.
    void (*put_prev_proc)(uint8_t const cpu, struct proc * proc);
};

// Initialize the scheduler. This must be called once.
void sched_init(void);

// Check whether or not the scheduler has been initialized.
// @return: true if the scheduler has been initialized, false otherwise.
bool sched_running(void);

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

// Pick the next proc to run and run it on the current cpu.
// This function does __NOT__ return.
// @param regs: The current values of the registers for the current process. If
// this param is NULL then this is ignored.
void sched_run_next_proc(struct register_save_area const * const regs);
