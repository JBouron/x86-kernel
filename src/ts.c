#include <sched.h>
#include <debug.h>

// The "Trivial Scheduler" (TS).
// The TS is a dead simple scheduler with a single runqueue shared between all
// cpus.

// The runqueue.
static struct list_node RUNQUEUE;

// A lock protecting the runqueue against concurrent access.
static spinlock_t RUNQUEUE_LOCK;

// Lock the RUNQUEUE_LOCK.
static void lock_runqueue(void) {
    spinlock_lock(&RUNQUEUE_LOCK);
}

// Unlock the RUNQUEUE_LOCK.
static void unlock_runqueue(void) {
    spinlock_unlock(&RUNQUEUE_LOCK);
}

// Get a pointer on the runqueue and lock it.
// @return: A pointer on RUNQUEUE.
static struct list_node *get_runqueue_and_lock(void) {
    lock_runqueue();
    return &RUNQUEUE; 
}

// Initialize the runqueue and lock.
static void ts_sched_init(void) {
    list_init(&RUNQUEUE);
    spinlock_init(&RUNQUEUE_LOCK);
}

// Enqueue a process.
// @param proc: The process to enqueue.
static void ts_enqueue_proc(struct proc * const proc) {
    struct list_node * const runqueue = get_runqueue_and_lock();

    list_add_tail(runqueue, &proc->rq);

    unlock_runqueue();
}

// Check if a process is currently enqueued in the runqueue.
// @param proc: The process we are looking for in `rq`.
// @return: true if proc is enqueued false otherwise.
static bool in_runqueue(struct proc const * const proc) {
    struct list_node * const rq = &RUNQUEUE;
    struct proc * it;
    list_for_each_entry(it, rq, rq) {
        if (it == proc) {
            return true;
        }
    }
    return false;
}

// Dequeue a process.
// @param proc: The process to dequeue.
static void ts_dequeue_proc(struct proc * const proc) {
    struct list_node * const runqueue = get_runqueue_and_lock();

    ASSERT(in_runqueue(proc) && runqueue);
    list_del(&proc->rq);

    unlock_runqueue();
}

// Update the current process.
static void ts_update_curr(void) {
}

// React to a scheduler tick.
static void ts_tick(void) {
    // For now, the TS scheduler performs one context switch per tick.
    sched_resched();
}

// Select the next process to be run on a cpu.
// If no process is available, this function will return NO_PROC.
// @return: The next process to run.
static struct proc *ts_pick_next_proc(void) {
    struct proc * next;
    struct list_node * const runqueue = get_runqueue_and_lock();

    if (!list_size(runqueue)) {
        next = NO_PROC;
    } else {
        next = list_first_entry(runqueue, struct proc, rq);
        list_del(&next->rq);
    }

    unlock_runqueue();
    return next;
}

struct sched const ts_sched = {
    .sched_init          = ts_sched_init,
    .enqueue_proc        = ts_enqueue_proc,
    .dequeue_proc        = ts_dequeue_proc,
    .update_curr         = ts_update_curr,
    .tick                = ts_tick,
    .pick_next_proc      = ts_pick_next_proc,
    // Put prev is equivalent to enqueue.
    .put_prev_proc       = ts_enqueue_proc,
};
