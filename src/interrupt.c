#include <interrupt.h>
#include <debug.h>
#include <memory.h>
#include <segmentation.h>
#include <lapic.h>
#include <spinlock.h>
#include <percpu.h>
#include <ipm.h>
#include <proc.h>
#include <sched.h>

// Interrupt gate descriptor.
union interrupt_descriptor_t {
    // The raw value.
    uint64_t value;
    struct {
        // Bits 0 to 15 of the offset of the interrupt handler.
        uint16_t offset_15_0 : 16;
        // The segment selector to use when calling the interrupt handler.
        uint16_t segment_selector : 16;
        // Reserved.
        uint8_t reserved : 5;
        // Should _always_ be 0.
        uint8_t zero : 3;
        // Should _always_ be 6. This indicates that this is an interrupt gate
        // descriptor.
        uint8_t six : 3;
        // If set 32 bit else 16 bit interrupt handler.
        uint8_t size : 1;
        // Should _always_ be 0.
        uint8_t zero2 : 1;
        // The privilege level to use when calling the interrupt handler.
        uint8_t privilege_level : 2;
        // Is the entry present in the IDT ?
        uint8_t present : 1;
        // Bits 16 to 31 of the offset of the interrupt handler.
        uint16_t offset_31_16 : 16;
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(union interrupt_descriptor_t) == 8, "");

// Initialize an union interrupt_descriptor_t given the information.
// @param desc: The union interrupt_descriptor_t to initialize/fill.
// @param offset: The offset of the interrupt handler for this interrupt
// descriptor.
// @param segment_sel: The segment selector to use when calling the interrupt
// handler.
// @param priv_level: The privilege level to use when calling the interrupt
// handler.
static void init_desc(union interrupt_descriptor_t * const desc,
                      uint32_t const offset,
                      union segment_selector_t const segment_sel,
                      uint8_t const priv_level) {
    desc->offset_15_0 = offset & 0xFFFF;
    desc->offset_31_16 = offset >> 16;
    desc->segment_selector = segment_sel.value;
    desc->reserved = 0;
    desc->zero = 0;
    desc->six = 6;
    desc->size = 1;
    desc->zero2 = 0;
    desc->privilege_level = priv_level;
    desc->present = 1;
}

// This is the IDT. For now it only contains the architectural
// exception/interrupt handlers.
#define IDT_SIZE 129
union interrupt_descriptor_t IDT[IDT_SIZE] __attribute__((aligned (8)));

// Array of global callback per vector.
int_callback_t GLOBAL_CALLBACKS[IDT_SIZE];
// Since any cpu can access the GLOBAL_CALLBACKS array, we need a lock.
DECLARE_SPINLOCK(GLOBAL_CALLBACKS_LOCK);
// The cpu-private callbacks.
DECLARE_PER_CPU(int_callback_t, local_callbacks[IDT_SIZE]);

// Get the address/offset of the interrupt handler for a given vector.
extern uint32_t get_interrupt_handler(uint8_t const vector);

// Save the register values of a process.
// @param proc: The process for which the register values should be saved.
// @param regs: The current values of the registers for `proc`.
static void save_registers(struct proc * const proc,
                           struct register_save_area const * const regs) {
    memcpy(&proc->registers_save, regs, sizeof(*regs));
}

// The generic interrupt handler. Eventually all the interrupts call the generic
// handler.
// @param frame: Information about the interrupt.
// @return: true if this interrupt was a nested interrupt, false otherwise.
bool generic_interrupt_handler(struct interrupt_frame const * const frame) {
    // This should have been taken care of by the IDT entry being an interrupt
    // gate.
    ASSERT(!interrupts_enabled());

    if (sched_running_on_cpu()) {
        struct proc * const curr = this_cpu_var(curr_proc);
        ASSERT(curr);
        if (!curr->interrupt_nest_level) {
            // The current process on this cpu just got interrupted, save its
            // registers into its struct proc.
            // This is not required per se, since the register values are onto the
            // kernel stack of the current process, however it does make testing and
            // debug easier.
            save_registers(curr, frame->registers);
        }
        curr->interrupt_nest_level++;
    }

    // Now that the nesting level has been taken care of we can safely enable
    // interrupts again.
    // Note: The Intel manual says:
    //  "This write [EOI] must occur at the end of the handler routine, sometime
    //  before the IRET instruction."
    // However it seems that doing it early does no create any issue, and Linux
    // does it early as well for IPIs.
    lapic_eoi();
    cpu_set_interrupt_flag(true);

    // From this point forward any interrupt can be received while processing
    // the current one.

    uint8_t const vector = frame->vector;
    ASSERT(vector < IDT_SIZE);

    // Check for local callback first.
    int_callback_t callback = this_cpu_var(local_callbacks)[vector];

    // Check for global callback.
    if (!callback) {
        callback = GLOBAL_CALLBACKS[vector];
    }

    if (callback) {
        callback(frame);
    } else {
        // No callback found.
        // Log some info about the interrupt.
        LOG("Interrupt with vector %d\n", frame->vector);
        LOG("error code = %x\n", frame->error_code);
        LOG("eip = %x\n", frame->eip);
        LOG("cs = %x\n", frame->cs);
        LOG("eflags = %x\n", frame->eflags);
        PANIC("Unexpected interrupt in kernel\n");
    }

    // We __MUST__ disable interrupts from this point forward. The reason is two
    // fold:
    //  - First we need to decrease the nest level, to avoid race conditions
    //  this must be done with interrupts disabled.
    //  - If this is not a nested interrupt, the scheduler will be called after
    //  this function returns. If interrupts are not enabled while doing so,
    //  another interrupt could come in, see nest_level == 0, be serviced and
    //  call the scheduler as well (since the nest_level == 0). This means that
    //  there are two "concurrent" calls to the scheduler AND the registers
    //  values of the first interrupt are lost and the current process will get
    //  the registers of the first interrupt (in the scheduler) saved instead.
    cpu_set_interrupt_flag(false);

    if (sched_running_on_cpu()) {
        // Update stats about the current process.
        sched_update_curr();

        // Check if we need another round of scheduling.
        schedule();
        this_cpu_var(curr_proc)->interrupt_nest_level--;
    }
    
    return false;
}

// Disable the legacy Programable Interrupt Controller. This is important as it
// might trigger interrupt in the range 0-15 which might confuse the interrupt
// handlers.
static void disable_pic(void) {
    cpu_outb(0xA1, 0xFF);
    cpu_outb(0x21, 0xFF);
}

void interrupt_init(void) {
    // Disable interrupts.
    cpu_set_interrupt_flag(false);

    // Zero the IDT, be safe.
    memzero(IDT, sizeof(IDT));

    // Fill each entry in the IDT with the corresponding interrupt_handler.
    // FIXME: The value 33 is hardcoded. What we want here is to initialize all
    // entry for which there is an handler available (for now this is up until
    // 34 and 128 for syscalls).
    for (uint8_t vector = 0; vector < 35; ++vector) {
        uint32_t const handler_offset = get_interrupt_handler(vector);
        ASSERT(handler_offset);
        init_desc(IDT + vector, handler_offset, kernel_code_selector(), 0);
    }

    // The syscall handler must be accessible through software interrupts from
    // ring 3 hence the DPL should be 3.
    uint32_t const syscall_handler = get_interrupt_handler(SYSCALL_VECTOR);
    init_desc(IDT + SYSCALL_VECTOR, syscall_handler, kernel_code_selector(), 3);

    // Load the IDT information into the IDTR of the cpu.
    struct idt_desc const desc = {
        .base = IDT,
        .limit = (IDT_SIZE * 8 - 1),
    };
    cpu_lidt(&desc);

    // Disabled the PIC, this avoid recieving interrupts from it that share
    // vector with architectural exceptions/interrupts.
    disable_pic();

    // From now on, it is safe to enable interrupts.

    // Zero the callback array. This has to be done before enabling interrupts
    // to avoid race conditions.
    memzero(GLOBAL_CALLBACKS, sizeof(GLOBAL_CALLBACKS));
}

void ap_interrupt_init(void) {
    // Simply load IDT into the IDTR of this AP.
    struct idt_desc const desc = {
        .base = IDT,
        .limit = (IDT_SIZE * 8 - 1),
    };
    cpu_lidt(&desc);
}

// Register a callback for an interrupt vector.
// @param vector: The vector that should trigger the interrupt callback once
// "raised".
// @param callback: The callback to be called everytime an interrupt with vector
// `vector` is received by a CPU.
// @param global: If set, the callback is registered as global, otherwise as
// local.
static void register_callback(uint8_t const vector,
                              int_callback_t const callback,
                              bool const global) {
    ASSERT(vector < IDT_SIZE);

    int_callback_t * const callbacks = global ?
        GLOBAL_CALLBACKS : this_cpu_var(local_callbacks);

    if (global) {
        spinlock_lock(&GLOBAL_CALLBACKS_LOCK);
    }

    // For now there can only be one callback per interrupt vector. Idempotent
    // registrations are ok.
    ASSERT(!callbacks[vector] || callbacks[vector] == callback);
    callbacks[vector] = callback;

    if (global) {
        spinlock_unlock(&GLOBAL_CALLBACKS_LOCK);
    }
}

// Remove a callback for a given vector.
// @param vector: The interrupt vector for which the callback should be removed.
// @param global: If set, remove the global callback registered for `vector`,
// otherwise remove the local callback.
static void delete_callback(uint8_t const vector, bool const global) {
    int_callback_t * const callbacks = global ?
        GLOBAL_CALLBACKS : this_cpu_var(local_callbacks);

    if (global) {
        spinlock_lock(&GLOBAL_CALLBACKS_LOCK);
    }

    callbacks[vector] = NULL;

    if (global) {
        spinlock_unlock(&GLOBAL_CALLBACKS_LOCK);
    }
}

void interrupt_register_global_callback(uint8_t const vector,
                                        int_callback_t const callback) {
    register_callback(vector, callback, true);
}

void interrupt_register_local_callback(uint8_t const vector,
                                       int_callback_t const callback) {
    register_callback(vector, callback, false);
}

void interrupt_delete_global_callback(uint8_t const vector) {
    delete_callback(vector, true);
}

void interrupt_delete_local_callback(uint8_t const vector) {
    delete_callback(vector, false);
}

bool interrupt_vector_has_error_code(uint8_t const vector) {
    return vector == 0x8 || vector == 0xA || vector == 0xB || vector == 0xC ||
        vector == 0xD || vector == 0xE || vector == 0x11;
}

#include <interrupt.test>
