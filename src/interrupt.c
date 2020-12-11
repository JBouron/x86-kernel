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

// Initialize an union interrupt_descriptor_t as an interrupt gate descriptor.
// @param segment_sel: The segment selector to use when calling the interrupt
// handler.
// @param offset: The offset of the interrupt handler for this interrupt
// descriptor.
// @param dpl: The privilege level to use when calling the interrupt handler.
#define IDT_ENTRY(segment_sel, offset, dpl)     \
    (union interrupt_descriptor_t){             \
        .offset_15_0 = (offset) & 0xFFFF,       \
        .offset_31_16 = (offset) >> 16,         \
        .segment_selector = (segment_sel).value,\
        .reserved = 0,                          \
        .zero = 0,                              \
        .six = 6,                               \
        .size = 1,                              \
        .zero2 = 0,                             \
        .privilege_level = (dpl),               \
        .present = 1,                           \
    }

// Initialize an union interrupt_descriptor_t as task gate descriptor.
// @param tss_segment_sel: The segment selector for the TSS to be used when
// handling the interrupt handler.
// @param offset: The offset of the interrupt handler for this interrupt
// descriptor.
// @param dpl: The privilege level to use when calling the interrupt handler.
#define IDT_TASK_GATE_ENTRY(tss_segment_sel, dpl)   \
    (union interrupt_descriptor_t){                 \
        .offset_15_0 = 0x0,                         \
        .offset_31_16 = 0x0,                        \
        .segment_selector = (tss_segment_sel).value,\
        .reserved = 0,                              \
        .zero = 0,                                  \
        .six = 5,                                   \
        .size = 0,                                  \
        .zero2 = 0,                                 \
        .privilege_level = (dpl),                   \
        .present = 1,                               \
    }

// The IDT contains an entry for every vector. Each handler is only 5 bytes long
// (a single call instruction) hence this is ok to have them even if not all of
// them are used.
#define IDT_SIZE 256
union interrupt_descriptor_t IDT[IDT_SIZE] __attribute__((aligned (8)));

// Array of global callback per vector.
int_callback_t GLOBAL_CALLBACKS[IDT_SIZE];
// Since any cpu can access the GLOBAL_CALLBACKS array, we need a lock.
DECLARE_SPINLOCK(GLOBAL_CALLBACKS_LOCK);
// The cpu-private callbacks.
DECLARE_PER_CPU(int_callback_t, local_callbacks[IDT_SIZE]);

// Get the address/offset of the interrupt handler for a given vector.
// @param vector: The vector of the handler to get.
// @return: The virtual offset of the handler for vector `vector`.
static uint32_t get_interrupt_handler(uint8_t const vector) {
    extern uint8_t interrupt_handler_0;
    return ((uint32_t)&interrupt_handler_0) + vector * 5;
}

// Dump the value of the registers at the time of the interrupt.
// @param frame: The interrupt frame for which the registers should be dumped.
static void dump_registers(struct interrupt_frame const * const frame) {
    struct register_save_area const * const regs = frame->registers;
    LOG("EAX = %x EBX = %x ECX = %x EDX = %x\n",
        regs->eax, regs->ebx, regs->ecx, regs->edx);
    LOG("ESI = %x EDI = %x EBP = %x ESP = %x\n",
        regs->esi, regs->edi, regs->ebp, regs->esp);
    LOG("EIP = %x EFL = %x\n", frame->eip, frame->eflags);
    LOG("ES  = %x CS  = %x SS  = %x DS  = %x\n",
        regs->es, regs->cs, regs->ss, regs->ds);
    LOG("FS  = %x GS  = %x\n", regs->fs, regs->gs);
    struct gdt_desc gdt;
    struct idt_desc idt;
    cpu_sgdt(&gdt);
    cpu_sidt(&idt);
    LOG("GDT = %x %x\n", gdt.base, gdt.limit);
    LOG("IDT = %x %x\n", idt.base, idt.limit);

    // Control registers should be the same as the time of the interrupt. FIXME.
    uint32_t const cr0 = cpu_read_cr0();
    uint32_t const cr2 = (uint32_t)cpu_read_cr2();
    uint32_t const cr3 = cpu_read_cr3();
    uint32_t const cr4 = cpu_read_cr4();
    LOG("CR0 = %x CR2 = %x CR3 = %x CR4 = %x\n", cr0, cr2, cr3, cr4);

    uint64_t const efer = read_msr(0xC0000080);
    LOG("EFER = %X\n", efer);
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
        dump_registers(frame);
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

        struct proc * const curr = this_cpu_var(curr_proc);
        bool const nested_interrupt = curr->interrupt_nest_level > 1;

        // Check if we need another round of scheduling. For now only do this if
        // we are not in a nested interrupt. That way a process cannot be
        // migrated to another cpu in the middle of a kernel operation and
        // percpu variable are always correct. FIXME: Add a system to disable
        // preemption when handling percpu vars.
        if (!nested_interrupt) {
            schedule();
        }
        curr->interrupt_nest_level--;
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

    LOG("Initializing interrupt table\n");

    // Zero the IDT, be safe.
    memzero(IDT, sizeof(IDT));

    union segment_selector_t const kcode_sel = kernel_code_selector();

    // Fill each entry in the IDT with the corresponding interrupt_handler.
    for (uint16_t vector = 0; vector < IDT_SIZE; ++vector) {
        if (vector == 8) {
            union segment_selector_t const df_sel = {
                .requested_priv_level = 0,
                .is_local = 0,
                .index = 5,
            };
            IDT[vector] = IDT_TASK_GATE_ENTRY(df_sel, 0);
        } else {
            uint32_t const handler_offset = get_interrupt_handler(vector);
            IDT[vector] = IDT_ENTRY(kcode_sel, handler_offset, 0);
        }
    }

    // The syscall handler must be accessible through software interrupts from
    // ring 3 hence the DPL should be 3.
    uint32_t const syscall_handler = get_interrupt_handler(SYSCALL_VECTOR);
    IDT[SYSCALL_VECTOR] = IDT_ENTRY(kcode_sel, syscall_handler, 3);

    // Load the IDT information into the IDTR of the cpu.
    struct idt_desc const desc = {
        .base = to_phys(IDT),
        .limit = (IDT_SIZE * 8 - 1),
    };
    LOG("IDTR = {.base = %p, .limit = %u}\n", desc.base, desc.limit);
    cpu_lidt(&desc);

    // Disabled the PIC, this avoid recieving interrupts from it that share
    // vector with architectural exceptions/interrupts.
    disable_pic();

    // From now on, it is safe to enable interrupts.

    // Zero the callback array. This has to be done before enabling interrupts
    // to avoid race conditions.
    memzero(GLOBAL_CALLBACKS, sizeof(GLOBAL_CALLBACKS));

    // We can already set the Double Fault callback to catch stack overflows in
    // kernel.
    interrupt_register_global_callback(0x8, double_fault_panic);
}

void interrupt_fixup_idtr(void) {
    struct idt_desc desc;
    cpu_sidt(&desc);
    desc.base = to_virt(desc.base);
    LOG("Fixup IDTR = {.base = %p, .limit = %u}\n", desc.base, desc.limit);
    cpu_lidt(&desc);
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
