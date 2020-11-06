#include <kmalloc.h>
#include <ipm.h>
#include <spinlock.h>
#include <rw_lock.h>

// Helper to run a "locking scenario", which is a step by step scenario used to
// simulate real usage.
// A scenario consists of a number of steps. Each step consists of a number of
// actions. All actions in a step are run in parallel on different cpus (other
// than the cpu running the test). Steps are executed one after the other, and
// the framework makes sure that a step completed before executing the next.

// The possible actions of a cpu in a step.
enum action {
    // Do nothing.
    NONE,
    // Acquire the lock (spinlock).
    SPINLOCK_LOCK,
    // Release the lock (spinlock).
    SPINLOCK_UNLOCK,
    // Acquire the read lock on a RW lock.
    RWLOCK_READ_LOCK,
    // Release the read lock on a RW lock.
    RWLOCK_READ_UNLOCK,
    // Acquire the write lock on a RW lock.
    RWLOCK_WRITE_LOCK,
    // Release the write lock on a RW lock.
    RWLOCK_WRITE_UNLOCK,
};

// An action in a step. This is executed by a single cpu.
struct step_action {
    // The lock. This is a void* so that this framework can work with other lock
    // implementation and not only spinlocks.
    void *lock;
    // The action to execute.
    enum action action;
    // Before performing the action, the cpu will wait for this variable to
    // become true. This is done so that all cpus in a given step starts their
    // action relatively at the same time.
    bool volatile *start_flag;
    // If the action is succesful, the cpu will write true to this field.
    bool volatile done;
};

// A step of a scenario. Each step contain one action per cpu.
struct step {
    // The size of the possible_outcomes array below.
    uint32_t num_possible_outcomes;
    // 2D array of the possible outcome. After the cpus have executed their
    // action, the framework will look at their result (the done field in the
    // struct step_action) and compare with the set of possible outcomes. This
    // array should be a num_possible_outcomes x num_cpus matrix.
    bool * possible_outcomes;
    // The number of actions in the step. This also means the number of cpus
    // used in the step.
    uint32_t num_actions;
    // The actions of the step.
    struct step_action actions[0];
};

// Create a step. This is not meant to be used directly, see create_step()
// instead.
// @param lock: The lock to use in the step.
// @param num_actions: The number of actions/cpus of the step.
// @param actions: Array containing the actions to be executed in this step.
// @param num_possible_outcomes: The number of possible outcomes for this step.
// @param possible_outcomes: The possible outcomes of this step.
// @return: An allocated and initialized struct step.
static struct step *_create_step(void * const lock,
                                uint32_t const num_actions,
                                enum action const * const actions,
                                uint32_t const num_possible_outcomes,
                                bool * const possible_outcomes) {
    size_t const size = sizeof(struct step) +
        num_actions * sizeof(struct step_action);
    struct step * const step = kmalloc(size);

    step->num_possible_outcomes = num_possible_outcomes;
    step->possible_outcomes = possible_outcomes;
    step->num_actions = num_actions;

    for (uint32_t i = 0; i < num_actions; ++i) {
        struct step_action * const action = &step->actions[i];
        action->lock = lock;
        action->action = actions[i];
        action->done = false;
    }

    return step;
}

// Create a step.
// @param lock: The lock to use in the step.
// @param num_actions: The number of actions in the step.
// @param num_possible_outcomes: The number of possible outcomes of the step.
// @param ...: The rest of the parameters are as follow:
//    - <num_action> enum action indicating what the actions of the step should
//    be.
//    - The matrix of possible outcomes stored in row-major order.
static struct step *create_step(void * const lock,
                                uint32_t const num_actions,
                                uint32_t const num_possible_outcomes,
                                ...) {
    va_list args;
    va_start(args, num_possible_outcomes);

    // Parse the actions.
    enum action * const actions = kmalloc(num_actions * sizeof(*actions));
    ASSERT(actions);
    for (uint32_t i = 0; i < num_actions; ++i) {
        actions[i] = va_arg(args, enum action);
    }

    // Parse the possible outcomes.
    size_t const po_size = num_possible_outcomes * num_actions;
    bool * const possible_outcomes = kmalloc(po_size * sizeof(bool));
    ASSERT(possible_outcomes);
    for (uint32_t i = 0; i < po_size; ++i) {
        possible_outcomes[i] = !!va_arg(args, int);
    }

    // Create the step from the info in the va_arg.
    struct step * const s = _create_step(lock,
                                         num_actions,
                                         actions,
                                         num_possible_outcomes,
                                         possible_outcomes);

    // The actions have been copied to the struct step_action's action field in
    // the struct step. Therefore the array is not used anymore. The
    // possible_outcomes matrix is still in use and should be freed when
    // deleting the struct step.
    kfree(actions);
    va_end(args);
    return s;
}

// Delete and free the memory used by a struct step.
// @param step: The struct step to delete and free.
static void delete_step(struct step * const step) {
    kfree(step->possible_outcomes);
    kfree(step);
}

// Run a struct step_action. This function is meant to be used with
// exec_remote_call to execute an action on a remote cpu.
// @param arg: A pointer on a struct step_action.
// The cpu will first wait on the start_flag of the step_action to become true
// then perform the action before writing true to the step_action's done field
// (provided that this action should be successful).
static void run_step_action_remote(void * const arg) {
    struct step_action * const step_action = arg;
    ASSERT(!step_action->done);

    uint8_t const cpu = cpu_id();
    bool volatile * const flag = step_action->start_flag;
    LOG("[%u] Waiting on start flag %p\n", cpu, flag);
    while (!*flag);

    // Make sure that interrupts are enabled if this cpu tries to acquire the
    // lock.
    cpu_set_interrupt_flag(true);

    switch (step_action->action) {
        case NONE:
            break;
        case SPINLOCK_LOCK:
            spinlock_lock((spinlock_t*)step_action->lock);
            LOG("[%u] Acquired spinlock\n", cpu);
            break;
        case SPINLOCK_UNLOCK:
            spinlock_unlock((spinlock_t*)step_action->lock);
            LOG("[%u] Released spinlock\n", cpu);
            break;
        case RWLOCK_READ_LOCK:
            rwlock_read_lock((rwlock_t*)step_action->lock);
            LOG("[%u] Acquired read lock on rwlock\n", cpu);
            break;
        case RWLOCK_READ_UNLOCK:
            rwlock_read_unlock((rwlock_t*)step_action->lock);
            LOG("[%u] Released read lock on rwlock\n", cpu);
            break;
        case RWLOCK_WRITE_LOCK:
            rwlock_write_lock((rwlock_t*)step_action->lock);
            LOG("[%u] Acquired write lock on rwlock\n", cpu);
            break;
        case RWLOCK_WRITE_UNLOCK:
            rwlock_write_unlock((rwlock_t*)step_action->lock);
            LOG("[%u] Released write lock on rwlock\n", cpu);
            break;
        default:
            PANIC("Unknown action");
            break;
    }

    step_action->done = true;

    // If the cpu acquired the lock then the interrupts got disabled. However we
    // need them enabled for the next step.
    cpu_set_interrupt_flag(true);
}

// Set the current cpu into an infinite loop with interrupts enabled. This is
// meant to stop the remote cpus at the end of a step before continuing to the
// next step.
static void standby(void * const unused) {
    while (true) {
        cpu_set_interrupt_flag_and_halt();
    }
}

// Run a step. This function will execute all the actions of the step on the
// remote cpu, wait for the action to be performed and set the remote cpu into a
// standby mode. Remote cpus will modify the done field of their respective
// struct step_action.
// @param step: The step to be executed.
static void run_step(struct step * const step) {
    // The flag that will be used to start all cpus "at the same time".
    bool volatile start_flag = false;

    // Set the pointer to the start flag in all actions.
    for (uint32_t i = 0; i < step->num_actions; ++i) {
        step->actions[i].start_flag = &start_flag;
    }

    // Now get all cpus ready to execute their action.
    for (uint32_t i = 0; i < step->num_actions; ++i) {
        uint8_t const cpu = TEST_TARGET_CPU(i);
        exec_remote_call(cpu, run_step_action_remote, &step->actions[i], false);
    }

    // Wait for a while for stragglers before signaling the start.
    lapic_sleep(100);

    // Give the signal to start.
    start_flag = true;

    // Each cpu has 1/2 second to perform its task.
    lapic_sleep(500);

    // Put the cpus in standby.
    for (uint32_t i = 0; i < step->num_actions; ++i) {
        uint8_t const cpu = TEST_TARGET_CPU(i);
        exec_remote_call(cpu, standby, NULL, false);
    }
}

// Check that a step was successful per its possible outcomes. This must be
// called after run_step().
// @param step: The step to check.
// @return: true if the step was successfull, that is if the observed results
// are part of the possible outcomes, false otherwise.
static bool check_step(struct step const * const step) {
    LOG("Outcome =");
    for (uint32_t i = 0; i < step->num_actions; ++i) {
        LOG(" %s", step->actions[i].done ? "true" : "false");
    }
    LOG("\n");

    for (uint32_t i = 0; i < step->num_possible_outcomes; ++i) {
        bool const * const candidate = step->possible_outcomes + i*step->num_actions;

        LOG("Possib. :");
        for (uint32_t j = 0; j < step->num_actions; ++j) {
            LOG(" %s", candidate[j] ? "true" : "false");
        }

        bool match = true;
        for (uint32_t j = 0; j < step->num_actions; ++j) {
            if (step->actions[j].done != candidate[j]) {
                match = false;
                break;
            }
        }
        LOG(" => %s\n", match ? "MATCH" : "no match");
        if (match) {
            return true;
        }
    }
    return false;
}

// Run and check a step.
// @param step: The step to run and check.
// @return: true if the step was successful, false otherwise.
static bool run_and_check_step(struct step * const step) {
    run_step(step);
    return check_step(step);
}
