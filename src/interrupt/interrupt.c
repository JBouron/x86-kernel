#include <interrupt/interrupt.h>
#include <utils/memory.h>
#include <asm/asm.h>
#include <utils/debug.h>
#include <interrupt/handlers.h>
#include <interrupt/apic/apic.h>

// The interrupt all use the kernel code segment.
static uint16_t const INT_CODE_SEGMENT = 0x8;

// The IDT gate descriptor. This format is what the processor expects in the
// IDT, as opposed to interrupt_desc_t which are meant to be used by the
// interrupt "API".
struct idt_gate_desc_t {
    // The offset to the handler.
    uint16_t offset_bits15_to_0 : 16;
    uint16_t segment_selector : 16; 
    // Those bits should not be touched.
    uint8_t reserved : 5;
    // Those bits *must* be 0. This is enforced by every function that
    // manipulates interrupt_gate_desc_t. Only used for interrupt and trap gate
    // descriptors.
    uint8_t zeros : 3;
    // This is the bits indicating wether this entry is a task gate, an
    // interrupt gate or a trap gate.
    uint8_t type : 5;
    // The level of privileges required to operate the gate.
    uint8_t privileges : 2;
    // Is this entry present in the IDT ?
    uint8_t present : 1;
    // The remaining of the offset.
    uint16_t offset_bits31to16 : 16;
};

// The actual IDT of 256 entries.
#define IDT_SIZE 256
static struct idt_gate_desc_t IDT[IDT_SIZE];

// Convert an interrupt_gate_t to a idt_gate_desc_t.
static void
_convert_idt_entry(struct interrupt_gate_t const * const from,
                   struct idt_gate_desc_t * const to) {
    uint32_t const offset = (uint32_t)from->offset;
    to->offset_bits15_to_0 = (offset & 0x0000FFFF);
    to->offset_bits31to16 = (offset & 0xFFFF0000) >> 16;
    to->zeros = 0x0;
    // For interrupt gates, the type should be "0D110" where D is '1' for 32-bit
    // handler. Thus in our case the type should be 0b01110
    to->type = 0b01110;
    to->privileges = 0; // Ring 0.
    to->present = 1;
    to->segment_selector = from->segment_selector;
}

void
idt_init(void) {
    // Zero the IDT to invalidate all entries.
    uint16_t byte_size = IDT_SIZE * sizeof(struct idt_gate_desc_t);
    memzero((uint8_t*)IDT, byte_size);

    // Here are all the mandatory exception handlers ordered by vector.
    void (*exception_handlers[])(void) = {
        interrupt_handler_0, interrupt_handler_1, interrupt_handler_2,
        interrupt_handler_3, interrupt_handler_4, interrupt_handler_5,
        interrupt_handler_6, interrupt_handler_7, interrupt_handler_8,
        interrupt_handler_9, interrupt_handler_10, interrupt_handler_11,
        interrupt_handler_12, interrupt_handler_13, interrupt_handler_14,
        interrupt_handler_15, interrupt_handler_16, interrupt_handler_17,
        interrupt_handler_18, interrupt_handler_19, interrupt_handler_20,
    };

    // Copy all the mandatory exception handlers one-by-one.
    size_t const len = sizeof(exception_handlers)/sizeof(exception_handlers[0]);
    for(size_t i = 0; i < len; ++i) {
        struct interrupt_gate_t gate = {
            .vector = i,
            .offset = exception_handlers[i],
            .segment_selector = INT_CODE_SEGMENT,
        };
        idt_register_handler(&gate);
    }

    // Create the table desc for the IDT and load it.
    struct table_desc_t idt_desc = {
        .base_addr = (uint8_t*)IDT,
        .limit = byte_size-1,
    };

    // Finally load the IDT.
    load_idt(&idt_desc);
}

void
idt_register_handler(struct interrupt_gate_t const * const handler) {
    struct idt_gate_desc_t desc;
    size_t const code = handler->vector;
    _convert_idt_entry(handler, &desc);

    // Copy the handler to IDT[code].
    uint8_t * const dest = (uint8_t*)(IDT + code);
    uint8_t * const src = (uint8_t*)&desc;
    memcpy(dest, src, sizeof(struct idt_gate_desc_t));
}

void
generic_interrupt_handler(struct interrupt_desc_t const * const desc) {
    LOG("Interrupt: {\n");
    LOG("   vector = %x\n", desc->vector);
    LOG("   error code = %x\n", desc->error_code);
    LOG("   eflags = %x\n", desc->eflags);
    LOG("   cs = %x\n", desc->cs);
    LOG("   eip = %x\n", desc->eip);
    LOG("}\n");
    // Signal the end of interrupt to the local APIC, this can be done before
    // the iret. 
    apic_eoi();
}
