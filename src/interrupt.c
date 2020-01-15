#include <interrupt.h>
#include <debug.h>
#include <memory.h>
#include <segmentation.h>
#include <lapic.h>

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
#define IDT_SIZE 33
union interrupt_descriptor_t IDT[IDT_SIZE] __attribute__((aligned (8)));

// All the code backs per interrupt vector. Ideally this should be a linked list
// but as we don't have a way to dynamically allocate memory for now a static
// array with one entry per vector should be enough.
int_callback_t CALLBACKS[IDT_SIZE];

// Get the address/offset of the interrupt handler for a given vector.
extern uint32_t get_interrupt_handler(uint8_t const vector);

// The generic interrupt handler. Eventually all the interrupts call the generic
// handler.
// @param frame: Information about the interrupt.
__attribute__((unused)) void generic_interrupt_handler(
    struct interrupt_frame_t const * const frame) {
    ASSERT(frame->vector <= 255);
    // A sanity check that the low level interrupt handler did not send garbage
    // to us.

    // Log some info about the interrupt.
    LOG("Interrupt with vector %d\n", frame->vector);
    LOG("error code = %x\n", frame->error_code);
    LOG("eip = %x\n", frame->eip);
    LOG("cs = %x\n", frame->cs);
    LOG("eflags = %x\n", frame->eflags);

    if (frame->vector < IDT_SIZE && CALLBACKS[frame->vector]) {
        // A callback has been registered for this interrupt vector, call it.
        CALLBACKS[frame->vector](frame);
    } else {
        PANIC("Unexpected interrupt in kernel\n");
    }
    lapic_eoi();
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
    for (uint8_t vector = 0; vector < IDT_SIZE; ++vector) {
        uint32_t const handler_offset = get_interrupt_handler(vector);
        ASSERT(handler_offset);
        init_desc(IDT + vector, handler_offset, kernel_code_selector(), 0);
    }

    // Load the IDT information into the IDTR of the cpu.
    struct idt_desc_t const desc = {
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
    memzero(CALLBACKS, sizeof(CALLBACKS));
}

void interrupt_register_callback(uint8_t const vector,
                                 int_callback_t const callback) {
    ASSERT(vector < IDT_SIZE);
    // For now there can only be one callback per interrupt vector.
    ASSERT(!CALLBACKS[vector]);
    CALLBACKS[vector] = callback;
}

void interrupt_delete_callback(uint8_t const vector) {
    CALLBACKS[vector] = NULL;
}

#include <interrupt.test>