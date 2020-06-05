#include <syscalls.h>
#include <percpu.h>
#include <debug.h>
#include <sched.h>

void do_exit(uint8_t const exit_code) {
    // For now, do_exit is trivial, we only set the state of the process to
    // dead. The exit code is stored, but not used yet.
    struct proc * const curr = this_cpu_var(curr_proc);
    LOG("[%u] Process %p is dead\n", this_cpu_var(cpu_id), curr);
    curr->exit_code = exit_code;
    curr->state_flags |= PROC_DEAD;

    // Now that the process is dead, run a round of scheduling. This will switch
    // to a new process.
    sched_run_next_proc(NULL);
    __UNREACHABLE__;
}
