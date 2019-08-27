#include <interrupt/idt/idt.h>
#include <interrupt/handlers.h>
#include <interrupt/interrupt.h>
#include <utils/memory.h>
#include <memory/segment.h>
#include <asm/asm.h>

// The actual IDT.
struct idt_entry_t IDT[IDT_SIZE];

// Fill an idt_entry_t.
static void
__generate_idt_entry(struct idt_entry_t * const desc,
                     uint32_t const handler_offset) {
    desc->offset_bits15_to_0 = (handler_offset & 0x0000FFFF);
    desc->offset_bits31to16 = (handler_offset & 0xFFFF0000) >> 16;
    desc->zeros = 0x0;
    // For interrupt gates, the type should be "0D110" where D is '1' for 32-bit
    // handler. Thus in our case the type should be 0b01110
    desc->type = 0b01110;
    desc->privileges = 0; // Ring 0.
    desc->present = 1;
    desc->segment_selector = INT_CODE_SEGMENT;
}

// Insert an entry in the IDT. This is used for the bare interrupt handler.
// Other, dynamic handlers (such as external devices handlers) should be added
// through the exposed interface in interrupt.h
static void
__idt_insert_entry(uint8_t const vector,
                   struct idt_entry_t const * const desc) {
    uint8_t * const dest = (uint8_t*)(IDT + vector);
    uint8_t * const src = (uint8_t*)desc;
    memcpy(dest, src, sizeof(*desc));
}

// Fill the IDT with the corresponding interrupt handlers for all 256 vectors.
static void
__idt_populate_all(void) {
    // Insert the first 20 interrupt handler corresponding to the reserved
    // intel interrupts.
    size_t const n_vectors = IDT_SIZE;
    for(size_t i = 0; i < n_vectors; ++i) {
        struct idt_entry_t desc;
        uint32_t const handler_offset = (uint32_t)INTERRUPT_HANDLERS[i];
        uint8_t const vector = i;

        __generate_idt_entry(&desc, handler_offset);
        __idt_insert_entry(vector, &desc);
    }

    // Create the table desc for the IDT and load it.
    uint16_t const byte_size = IDT_SIZE * sizeof(IDT[0]);
    struct table_desc_t idt_desc = {
        .base_addr = (uint8_t*)IDT,
        .limit = byte_size-1,
    };

    // Finally load the IDT.
    load_idt(&idt_desc);
}

void
idt_init(void) {
    // Zero the IDT to invalidate all entries.
    uint16_t const byte_size = IDT_SIZE * sizeof(IDT[0]);
    memzero((uint8_t*)IDT, byte_size);

    // Populate the IDT with all interrupt handlers.
    __idt_populate_all();
}
