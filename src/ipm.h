#pragma once
#include <types.h>
#include <list.h>
#include <lock.h>
#include <percpu.h>

// To make communication between cores easier, the kernel provide a mechanism of
// Inter-Processor-Messaging (IPM).
// This mechanism allows one to send "messages" to remote cores through IPIs.
// When a core wants to send a message to a remote core, it first enqueue the
// message in the remote core's message queue (think of it as an inbox) and
// sends an IPI to the remote core.
// Upon receiving the IPI, the remote core will process the message and carry
// out any operation associated with it.
// IPIs are set to maskable, such that a core will not receive IPM message in a
// middle of a section judged non re-entrant.

// Each message is associated a tag indicating its nature.
enum ipm_tag_t {
    // This tag is used for testing only!
    __TEST,
    REMOTE_CALL,
};

// The structure of a message.
struct ipm_message_t {
    // The tag of this message.
    enum ipm_tag_t tag;
    // The APIC ID of the cpu who sent this message.
    uint8_t sender_id;
    // Any data associated with the message. This field is optional and can be
    // set to NULL if unused.
    void * data;
    // The length of the data memory region. If Any.
    size_t len;
    // Each message is enqueued in the remote cpu's message queue.
    struct list_node msg_queue;
} __attribute__((packed));

// The message queue of a cpu.
DECLARE_PER_CPU(struct list_node, message_queue);

// Since multiple cpus can try to send a message to the same destination
// concurrently, the message queue needs a lock to protect it against race
// conditions.
DECLARE_PER_CPU(struct spinlock_t, message_queue_lock);

// Initialize IPM related data i.e. per-cpu variables related to IPM. This must
// be done by a single cpu (not necessarily the BSP) _before_ attempting to send
// a message.
void init_ipm(void);

// Send an IPM to a remote cpu.
// @param cpu: The remote cpu to send the message to.
// @param tag: The tag of the message.
// @param data: The data of the message. NULL if not applicable.
// @param len: The length of the data area.
// Note: This function will wait for the send to be successful before returning.
void send_ipm(uint8_t const cpu,
              enum ipm_tag_t const tag,
              void * const data,
              size_t const len);

// Remote calls
// ============
//      The IPM mechanism provides a way to execute function calls on remote cpu
// through a REMOTE_CALL message. A REMOTE_CALL message contains information
// about the call to be performed in it data field. This information is encoded
// in a struct remote_call_t.
// Upon processing the message, the remote cpu will execute the function call.

// Structure holding information for a remote call.
struct remote_call_t {
    // The function to call on the remote processor.
    void (*func)(void*);
    // The argument to pass to the above function.
    void *arg;
};

// Execute a function call on a remote processor.
// @param cpu: The APIC ID of the cpu on which the remote call is to be
// performed. Can be the current cpu.
// @param call: struct remote_call_t containing information about the remote
// call to carry out on the remote cpu.
// Note: This function will return once the message has been sent to the remote
// cpu. However there is no guarantee that the remote call completes before this
// function returns.
void exec_remote_call(uint8_t const cpu,
                      struct remote_call_t const * const call);

// Execute IPM related tests.
void ipm_test(void);
