#include <error.h>
#include <debug.h>
#include <kmalloc.h>
#include <sched.h>

// The linked list of error descs.
DECLARE_PER_CPU(struct list_node, error_list);

// Indicate if the error_list linked list has been initialized for this cpu. The
// list is initialized upon the first call to SET_ERROR().
DECLARE_PER_CPU(bool, error_list_initialized) = false;

// Each cpu also has a few satically allocated struct error_desc. Those are only
// reserved in case an error happens in a function that was called (in)directly
// by kmalloc(). This is required because calling kmalloc() in such a scenario
// could lead to an infinite loop.
#define NUM_STATIC_ERR_DESC 4
DECLARE_PER_CPU(struct error_desc, static_error_desc[NUM_STATIC_ERR_DESC]);

// Get the value of the IF flag in EFLAGS and disable interrupts.
// @return: the value of IF before disabling interrupts.
static bool get_irq_flag_and_disable(void) {
    bool const irq_enabled = interrupts_enabled();
    cpu_set_interrupt_flag(false);
    return irq_enabled;
}

// Reset the statically allocated error desc for the current cpu by setting them
// inactive.
static void reset_statically_allocated_error_desc(void) {
    for (uint32_t i = 0; i < NUM_STATIC_ERR_DESC; ++i) {
        this_cpu_var(static_error_desc)[i].is_static = true;
        this_cpu_var(static_error_desc)[i].active = false;
    }
}

// Initialize the error mechanism on the current cpu.
static void init_error_mechanism(void) {
    preempt_disable();
    list_init(&this_cpu_var(error_list));
    reset_statically_allocated_error_desc();
    this_cpu_var(error_list_initialized) = true;
    preempt_enable();
}

// Get the address of a free statically allocated struct error_desc for this
// cpu.
// @return: If such an error_desc is available this function returns its
// address, otherwise false is returned.
static struct error_desc *get_static_error_desc(void) {
    ASSERT(!interrupts_enabled());
    for (uint32_t i = 0; i < NUM_STATIC_ERR_DESC; ++i) {
        struct error_desc * const d = this_cpu_var(static_error_desc) + i;
        if (!d->active) {
            return d;
        }
    }
    return NULL;
}

void _set_error(char const * const file,
                uint32_t const line,
                char const * const func,
                char const * message,
                error_code_t const error_code) {
    // Interrupt must be disabled so that we don't end up in a race condition
    // trying to insert two struct error_desc at the same location on the linked
    // list.
    bool const irq = get_irq_flag_and_disable();

    LOG("----[ CPU %u ERROR! ]----\n", cpu_apic_id());
    LOG("In function %s @ %s:%u\n", func, file, line);
    LOG("Error (%d): %s\n", error_code, message);

    if (!this_cpu_var(error_list_initialized)) {
        init_error_mechanism();
    }

    struct error_desc * desc;
    if (this_cpu_var(kmalloc_nest_level) > 0) {
        // If the error is happening under a serie of nested calls originating
        // from kmalloc(). We need to use a statically allocated error_desc.
        desc = get_static_error_desc();
        if (!desc) {
            LOG("No more static error_desc available");
            goto reset_irq_and_ret;
        }
    } else {
        desc = kmalloc(sizeof(*desc));
        if (!desc) {
            // Nothing we can do here.
            goto reset_irq_and_ret;
        }
    }
    
    struct list_node * const error_list = &this_cpu_var(error_list);

    error_code_t true_error_code = error_code;
    if (error_code == ENONE) {
        // Replace special error code ENONE with the previous error code.
        if (!list_size(error_list)) {
            PANIC("ENONE used in first error.");
        }

        struct error_desc const * const prev =
            list_last_entry(error_list, struct error_desc, error_linked_list);
        true_error_code = prev->error_code;
    }

    desc->active = true;
    desc->file = file;
    desc->line = line;
    desc->func = func;
    desc->message = message;
    desc->error_code = true_error_code;
    list_init(&desc->error_linked_list);

    list_add_tail(error_list, &desc->error_linked_list);

reset_irq_and_ret:
    cpu_set_interrupt_flag(irq);
}

void _clear_error(void) {
    // Interrupt must be disabled while flushing the linked list to avoid race
    // conditions.
    bool const irq = get_irq_flag_and_disable(); 

    ASSERT(this_cpu_var(error_list_initialized));

    // Remove all entries in the linked list.
    struct list_node * const err_list = &this_cpu_var(error_list);
    while (list_size(err_list)) {
        struct error_desc * const desc =
            list_first_entry(err_list, struct error_desc, error_linked_list);
        list_del(&desc->error_linked_list);
        if (!desc->is_static) {
            kfree(desc);
        }
    }

    reset_statically_allocated_error_desc();
    cpu_set_interrupt_flag(irq);
}

#include <error.test>
