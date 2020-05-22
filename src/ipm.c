#include <ipm.h>
#include <lapic.h>
#include <interrupt.h>
#include <acpi.h>
#include <debug.h>
#include <kmalloc.h>

// This structure contains all the state necesasry to execute a remote function.
// This represents the payload of an IPM message with tag REMOTE_CALL.
// There is a single instance of this structure associated with a remote call,
// this means that N remote cores executing the function will have access to the
// same instance and therefore should not modify it.
struct remote_call_data {
    // The function to execute.
    void (*func)(void*);
    // The argument to pass to the function upon execution.
    void *arg;
    // A ref count of this instance. If the ref_counts become 0 then it is the
    // responsibility of the core that updated it to 0 to free this structure.
    atomic_t ref_count;
};

// Lock the message queue of the current CPU.
static void lock_message_queue(void) {
    spinlock_lock(&this_cpu_var(message_queue_lock));
}

// Unlock the message queue of the current CPU.
static void unlock_message_queue(void) {
    spinlock_unlock(&this_cpu_var(message_queue_lock));
}

// Used for testing purposes only. This callback is called whenever a message
// with __TEST tag is received.
static void (*TEST_TAG_CALLBACK)(struct ipm_message const *) = NULL;

// Handle a remote call from an IPM message.
// @param call: The struct remote_call_data associated with the remote call.
static void handle_remote_call(struct remote_call_data * const call) {
    if(atomic_read(&call->ref_count) == 0) {
        // There is a problem here, the ref_count of the call is 0, which means
        // all targeted cpus have executed this call already and the sender
        // might have moved on. Hence, we are not supposed to execute !
        PANIC("Try to exec with ref_count = 0");
    }

    // Enable interrupts when running the function. This is necessary to avoid
    // deadlocks if the function is looping and a critical message comes in (TLB
    // for instance as the sender will wait for our ack).
    bool const irqs = interrupts_enabled();
    cpu_set_interrupt_flag(true);
    call->func(call->arg);
    cpu_set_interrupt_flag(irqs);

    if (atomic_dec_and_test(&call->ref_count)) {
        // This is the job of this cpu to free the remote_call_data_t.
        kfree(call);
    }
    // WARNING: Even if we did not free the remote_call_data_t ourselves, we
    // cannot use `call` anymore, as it might have been freed by somebody else
    // at this point.
}

// Process any message in this cpu's message queue.
static void process_messages(void) {
    uint8_t const cpuid = this_cpu_var(cpu_id);
    struct list_node * const head = &this_cpu_var(message_queue);

    // Atomically pick the first message in the queue and process it. Do that
    // until all messages have been processed.
    lock_message_queue();
    while (list_size(head)) {
        // Get the first message in the queue and remove it.
        struct ipm_message * const msg =
            list_first_entry(head, struct ipm_message, msg_queue);
        struct list_node * const node = &msg->msg_queue;
        list_del(node);

        // We can now unlock the queue so that remote cpus can still send us
        // message while we are processing this one.
        unlock_message_queue();

        LOG("[%u] Processing message from %u\n", cpuid, msg->sender_id);
        switch (msg->tag) {
            case __TEST : {
                if (TEST_TAG_CALLBACK) {
                    TEST_TAG_CALLBACK(msg);
                }
                break;
            }
            case REMOTE_CALL : {
                struct remote_call_data * const call = msg->data;
                handle_remote_call(call);
                break;
            }
            case TLB_SHOOTDOWN : {
                // See exec_tlb_shootdown() for more information about how
                // TLB-shootdowns are implemented in this kernel.
                atomic_t * const wait = (atomic_t*)msg->data;
                cpu_invalidate_tlb();
                atomic_dec(wait);
                break;
            }
        }

        if (msg->receiver_dealloc) {
            kfree(msg);
        }

        // Re-acquire the lock before the next iteration since the condition
        // accesses the queue.
        lock_message_queue();
    }
    unlock_message_queue();
}

// The IPI handler for IPMs. This handler is registered globally by the init_ipm
// function.
// @param frame: The interrput frame. This is of very little use in this
// handler.
static void ipm_handler(struct interrupt_frame const * const frame) {
    ASSERT(frame);
    // Jump to the process_messages function to processes any message(s) in the
    // message queue.
    process_messages();
}

void init_ipm(void) {
    uint8_t const ncpus = acpi_get_number_cpus();
    // For each cpu, initialize the lock (to set it in an unlocked state) and
    // message queue.
    for (uint8_t cpu = 0; cpu < ncpus; ++cpu) {
        // This is technically an abomination since we don't own the lock, but
        // this work very will in practice.
        spinlock_unlock(&cpu_var(message_queue_lock, cpu));
        list_init(&cpu_var(message_queue, cpu));
    }

    // Register a global callback so that all cpus can receive IPMs.
    interrupt_register_global_callback(IPM_VECTOR, ipm_handler);
}

// Dynamically allocate an ipm_message_t and initialize it to the given values.
// Other fields in the struct are set to ""sane"" defaults, however it is
// recommended to set them yourself!
// @param tag: The tag of the message.
// @param data: The data of the message.
// @param len: The length of the data area of the message.
// @return: A pointer on an allocated struct ipm_message initialized to the
// values above.
static struct ipm_message * alloc_message(enum ipm_tag_t const tag,
                                            void * const data,
                                            size_t const len) {
    struct ipm_message * const message = kmalloc(sizeof(*message));
    message->tag = tag;
    message->sender_id = this_cpu_var(cpu_id);
    // By default make the receiving cpu deallocate the ipm_message_t. This is
    // because the sender has no idea when a particular message has been
    // procesed by the destination.
    message->receiver_dealloc = true;
    message->data = data;
    message->len = len;
    list_init(&message->msg_queue);
    return message;
}

// Enqueue a message on a cpu's message queue.
// @param message: The message to enqueue.
static void enqueue_message(struct ipm_message * const message,
                            uint8_t const cpu) {
    struct list_node * const message_queue = &cpu_var(message_queue, cpu);
    // Atomically append the message to the remote cpu's queue.
    spinlock_t * const lock = &cpu_var(message_queue_lock, cpu);
    spinlock_lock(lock);

    if (message->tag == TLB_SHOOTDOWN) {
        // TLB_SHOOTDOWNs are critical and should be handled as soon as
        // possible. Therefore enqueue them at the head of the message queue of
        // the destination.
        list_add(message_queue, &message->msg_queue);
    } else {
        list_add_tail(message_queue, &message->msg_queue);
    }

    spinlock_unlock(lock);
    LOG("[%u] Sending message to %u\n", this_cpu_var(cpu_id), cpu);
}

// Send an IPM.
// @param cpu: The remote cpu to send the message to. If this field is
// IPI_BROADCAST then send the IPM to all cpus except the current one.
// @param tag: The tag of the message.
// @param data: The data of the message. NULL if not applicable.
// @param len: The length of the data area.
// Note: This function will wait for the send to be successful before returning.
static void do_send_ipm(uint8_t const cpu,
                        enum ipm_tag_t const tag,
                        void * const data,
                        size_t const len) {
    uint8_t const ncpus = acpi_get_number_cpus();
    bool const is_broadcast = cpu == IPI_BROADCAST;

    uint8_t const start = is_broadcast ? 0 : cpu;
    uint8_t const end = is_broadcast ? ncpus : cpu + 1;
    for (uint8_t i = start; i < end; ++i) {
        if (is_broadcast && i == this_cpu_var(cpu_id)) {
            // Don't send the message to ourselves if we request a broadcast.
            continue;
        }
        struct ipm_message * const message = alloc_message(tag, data, len);
        enqueue_message(message, i);
    }

    // Now notify the remote cpu that a new message is in its message queue.
    // Even if many other cpus are trying to send an IPI to this particular
    // remote core, some IPIs may be dropped but the remote cpu will recieve at
    // least one, which is enough anyway.
    // Note: If the message is broadcast, the cpu will be IPI_BROADCAST and
    // therefore, the IPI will be sent to all cpus.
    lapic_send_ipi(cpu, IPM_VECTOR);
}

void send_ipm(uint8_t const cpu,
              enum ipm_tag_t const tag,
              void * const data,
              size_t const len) {
    do_send_ipm(cpu, tag, data, len);
}

void broadcast_ipm(enum ipm_tag_t const tag,
                   void * const data,
                   size_t const len) {
    // Rather than calling do_send_ipm for each cpu, call do_send_ipm with the
    // "broadcast cpu id". This way we use only a single IPI to notify all cpus
    // at once. This is cheaper than sending one IPI per cpu.
    do_send_ipm(IPI_BROADCAST, tag, data, len);
}

void exec_remote_call(uint8_t const cpu,
                      void (*func)(void*),
                      void * const arg,
                      bool const wait) {
    // How to choose the reference count initial value ?
    // ref_count == 1 => The remote core will execute the function and free the
    // struct remote_call_data immediately after. This is good if we don't
    // want to wait for the remote call to finish.
    // ref_count == 2 => After executing the call, the remote core will dec the
    // ref_count to 1, and therefore will not free the memroy. This is good if
    // we want to wait for the call to finish, just wait for the ref_count to
    // become one.
    int32_t const ref_count_initial_val = wait ? 2 : 1;

    struct remote_call_data *rem_data = kmalloc(sizeof(*rem_data));
    rem_data->func = func;
    rem_data->arg = arg;
    atomic_init(&rem_data->ref_count, ref_count_initial_val);

    send_ipm(cpu, REMOTE_CALL, rem_data, sizeof(rem_data));

    if (wait) {
        while (atomic_read(&rem_data->ref_count) != 1) {
            cpu_pause();
        }
        kfree(rem_data);
    }
}

void broadcast_remote_call(void (*func)(void*),
                           void * const arg,
                           bool const wait) {
    // As in exec_remote_call, the initial value of the ref_count depends on
    // whether or not we want for this cpu to wait for the remote calls to
    // finish.
    int32_t const ref_count_initial_val = acpi_get_number_cpus() - (wait?0:1);

    struct remote_call_data *rem_data = kmalloc(sizeof(*rem_data));
    rem_data->func = func;
    rem_data->arg = arg;
    atomic_init(&rem_data->ref_count, ref_count_initial_val);

    broadcast_ipm(REMOTE_CALL, rem_data, sizeof(rem_data));

    if (wait) {
        while (atomic_read(&rem_data->ref_count) != 1) {
            cpu_pause();
        }
        kfree(rem_data);
    }
}

void exec_tlb_shootdown(void) {
    // TLB-Shootdowns are implemented as follows:
    // For each remote cpu:
    //  1. Send an IPM message with tag = TLB_SHOOTDOWN. The data contains a
    //  pointer to an atomic_t initialized to 1.
    //  2. Wait for the atomic_t to become 0, this indicate that the remtoe cpu
    //  flushed its TLB.
    //
    // Upon receiving a TLB_SHOOTDOWN message, a core flushes its TLB and
    // decreases the value of the atomic_t passed through the data of the
    // message.

    atomic_t wait;

    // We _must_ statically allocate the message here as kmalloc could
    // potentially require a new group and therefore map a new page and execute
    // a TLB shootdown leading to an infinite loop.
    struct ipm_message tlb_message = {
        .tag = TLB_SHOOTDOWN,
        .sender_id = this_cpu_var(cpu_id),
        .receiver_dealloc = false,
        .data = &wait,
        .len = sizeof(wait),
    };

    // For now, the TLB shootdown message is sent to each cpu sequentially. This
    // is because of a limitation of IPM messages that can only be enqueued in a
    // single message queue at any time. This is slow but will do for now.
    for (uint8_t cpu = 0; cpu < acpi_get_number_cpus(); ++cpu) {
        if (cpu == this_cpu_var(cpu_id)) {
            continue;
        }

        atomic_init(&wait, 1);
        list_init(&tlb_message.msg_queue);

        // We need to manually enqueue and raise the IPI here as it is normally
        // done by send_ipm() which we cannot use here (because of the dynamic
        // allocation).
        enqueue_message(&tlb_message, cpu);
        lapic_send_ipi(cpu, IPM_VECTOR);

        // Now wait for the remote cpu to acknowlege the TLB_SHOOTDOWN message.
        // WARNING: While waiting for the remote CPU to acknowlege, we _MUST_
        // re-enable interrupts. This is to avoid a deadlock if the remote cpu
        // tries to send us a TLB_SHOOTDOWN message in the meantime.
        bool const irqs = interrupts_enabled();
        cpu_set_interrupt_flag(true);
        lapic_eoi();

        while (atomic_read(&wait)) {
            cpu_pause();
        }

        cpu_set_interrupt_flag(irqs);
    }
}

#include <ipm.test>
