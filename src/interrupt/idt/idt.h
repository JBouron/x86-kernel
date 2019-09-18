// Some functions to handle the Interrupt Descriptor Table of the processor.
#pragma once

#include <utils/types.h>

// The interrupt all use the kernel code segment.
static uint16_t const INT_CODE_SEGMENT = 0x8;

// The IDT gate descriptor. This is one entry of the IDT.
struct idt_entry {
    // The offset to the handler.
    uint16_t offset_bits15_to_0 : 16;
    uint16_t segment_selector : 16; 
    // Those bits should not be touched.
    uint8_t reserved : 5;
    // Those bits *must* be 0.
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
extern struct idt_entry IDT[IDT_SIZE];

// Initialize the IDT on the processor. This will fill the IDT with all the
// interrupt handlers for all vectors and set the present bit to 1 for all of
// them. It is then the responsability of the generic_interrupt_handler to
// detect invalid interrupts.
void
idt_init(void);
