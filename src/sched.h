#pragma once
#include <proc.h>

// The scheduler. For now limited to idle process. More to come later.

// Each cpu has a pointer on the process it is currently running.
DECLARE_PER_CPU(struct proc *, curr_proc);

// Initialize the scheduler. This must be called once.
void sched_init(void);

// Check whether or not the scheduler has been initialized.
// @return: true if the scheduler has been initialized, false otherwise.
bool sched_running(void);

// Start the scheduler. The cpu will start executing the first process it finds
// or go into idle.
// This function does __NOT__ return.
void sched_start(void);

// Update the current process of the cpu. This function must be called from the
// interrupt handler.
// @param regs: The latest values of the curr_proc's registers.
void sched_update_curr(struct register_save_area const * const regs);

// Pick the next proc to run and run it.
// This function does __NOT__ return.
void sched_run_next_proc(void);
