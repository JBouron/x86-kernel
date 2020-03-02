#include <ipm.h>
#include <lapic.h>
#include <interrupt.h>
#include <acpi.h>
#include <debug.h>
#include <kmalloc.h>

// Vector 33 is reserved to the IPM handler. This handler will process messages
// in the queue.
#define IPM_VECTOR  33

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
static void (*TEST_TAG_CALLBACK)(struct ipm_message_t const *) = NULL;

// Process any message in this cpu's message queue.
static void process_messages(void) {
    uint8_t const cpuid = this_cpu_var(cpu_id);
    struct list_node * const head = &this_cpu_var(message_queue);

    // Atomically pick the first message in the queue and process it. Do that
    // until all messages have been processed.
    lock_message_queue();
    while (list_size(head)) {
        struct ipm_message_t * const msg = list_first_entry(head, struct ipm_message_t, msg_queue);
        struct list_node * const node = &msg->msg_queue;
        list_del(node);

        // We can now unlock the queue so that remote cpus can still send us
        // message while we are processing this one.
        unlock_message_queue();

        LOG("[%u] Processing message from %u\n", cpuid, msg->sender_id);
        if (msg->tag == __TEST && TEST_TAG_CALLBACK) {
            TEST_TAG_CALLBACK(msg);
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
static void ipm_handler(struct interrupt_frame_t const * const frame) {
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
// @param tag: The tag of the message.
// @param data: The data of the message.
// @param len: The length of the data area of the message.
// @return: A pointer on an allocated struct ipm_message_t initialized to the
// values above.
static struct ipm_message_t * alloc_message(enum ipm_tag_t const tag,
                                            void * const data,
                                            size_t const len) {
    struct ipm_message_t * const message = kmalloc(sizeof(*message));
    message->tag = tag;
    message->sender_id = this_cpu_var(cpu_id);
    message->data = data;
    message->len = len;
    list_init(&message->msg_queue);
    return message;
}

void send_ipm(uint8_t const cpu,
              enum ipm_tag_t const tag,
              void * const data,
              size_t const len) {
    struct ipm_message_t * const message = alloc_message(tag, data, len);
    struct list_node * const message_queue = &cpu_var(message_queue, cpu);

    // Atomically append the message to the remote cpu's queue.
    struct spinlock_t * const lock = &cpu_var(message_queue_lock, cpu);
    spinlock_lock(lock);
    list_add_tail(message_queue, &message->msg_queue);
    spinlock_unlock(lock);

    LOG("[%u] Sending message to %u\n", this_cpu_var(cpu_id), cpu);

    // Now notify the remote cpu that a new message is in its message queue.
    // Even if many other cpus are trying to send an IPI to this particular
    // remote core, some IPIs may be dropped but the remote cpu will recieve at
    // least one, which is enough anyway.
    lapic_send_ipi(cpu, IPM_VECTOR);
}

#include <ipm.test>
