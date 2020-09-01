#include <error.h>
#include <percpu.h>
#include <debug.h>
#include <kmalloc.h>

// The linked list of error descs.
DECLARE_PER_CPU(struct list_node, error_list);

// Indicate if the error_list linked list has been initialized for this cpu. The
// list is initialized upon the first call to SET_ERROR().
DECLARE_PER_CPU(bool, error_list_initialized) = false;

// Get the value of the IF flag in EFLAGS and disable interrupts.
// @return: the value of IF before disabling interrupts.
static bool get_irq_flag_and_disable(void) {
    bool const irq_enabled = interrupts_enabled();
    cpu_set_interrupt_flag(false);
    return irq_enabled;
}

void _set_error(char const * const file,
                uint32_t const line,
                char const * const func,
                char const * message,
                int32_t const error_code) {
    // Interrupt must be disabled so that we don't end up in a race condition
    // trying to insert two struct error_desc at the same location on the linked
    // list.
    bool const irq = get_irq_flag_and_disable();

    LOG("----[ CPU %u ERROR! ]----\n", cpu_apic_id());
    LOG("In function %s @ %s:%u\n", func, file, line);
    LOG("Error (%d): %s\n", error_code, message);

    if (!percpu_initialized()) {
        // No chain of errors if percpu is not initialized.
        cpu_set_interrupt_flag(irq);
        return;
    }

    if (!this_cpu_var(error_list_initialized)) {
        list_init(&this_cpu_var(error_list));
        this_cpu_var(error_list_initialized) = true;
    }

    // TODO: Handle the case where the error comes from kmalloc() or under.
    struct error_desc * const desc = kmalloc(sizeof(*desc));
    if (!desc) {
        // Nothing we can do here.
        cpu_set_interrupt_flag(irq);
        return;
    }
    desc->active = true;
    desc->file = file;
    desc->line = line;
    desc->func = func;
    desc->message = message;
    desc->error_code = error_code;
    list_init(&desc->error_linked_list);

    list_add_tail(&this_cpu_var(error_list), &desc->error_linked_list);
    cpu_set_interrupt_flag(irq);
}

void _clear_error(void) {
    if (!percpu_initialized()) {
        return;
    }
    // Interrupt must be disabled while flushing the linked list to avoid race
    // conditions.
    bool const irq = get_irq_flag_and_disable(); 

    ASSERT(this_cpu_var(error_list_initialized));

    struct list_node * const err_list = &this_cpu_var(error_list);
    while (list_size(err_list)) {
        struct error_desc * const desc =
            list_first_entry(err_list, struct error_desc, error_linked_list);
        list_del(&desc->error_linked_list);
        kfree(desc);
    }

    cpu_set_interrupt_flag(irq);
}
