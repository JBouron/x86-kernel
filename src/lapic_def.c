#include <debug.h>

typedef struct __attribute__((packed)) {
    uint32_t val;
    uint8_t padding[12];
} lapic_reg_32_t;
STATIC_ASSERT(sizeof(lapic_reg_32_t) == 16, "");

typedef struct __attribute__((packed)) {
    lapic_reg_32_t bits_0_31;
    lapic_reg_32_t bits_32_63;
} lapic_reg_64_t;
STATIC_ASSERT(sizeof(lapic_reg_64_t) == 32, "");

typedef struct __attribute__((packed)) {
    lapic_reg_32_t bits_31_0;
    lapic_reg_32_t bits_63_32;
    lapic_reg_32_t bits_95_64;
    lapic_reg_32_t bits_127_96;
    lapic_reg_32_t bits_159_128;
    lapic_reg_32_t bits_191_160;
    lapic_reg_32_t bits_223_192;
    lapic_reg_32_t bits_255_224;
} lapic_reg_256_t;
STATIC_ASSERT(sizeof(lapic_reg_256_t) == 128, "");

// Interrupt Command Register related types. The ICR is used to send Inter
// Processor Interrupts (IPIs).

// Indicate the delivery mode of an IPI.
enum delivery_mode_t {
    // Deliver to processor(s) in destination field.
    NORMAL = 0,
    // Deliver to the processor with lowest priority.
    LOW_PRIO = 1,
    // Deliver a SMI. Vector must be 0.
    SYSTEM_MANAGEMENT_INTERRUPT = 2,
    // Deliver an NMI. Vector is ignored.
    NON_MASKABLE = 4,
    // Deliver to processor(s) in destination field with an INIT interrupt.
    // Vector must be 0.
    INIT = 5,
    // Send a startup SIPI to the destination(s).
    STARTUP = 6,
} __attribute__((packed));

// Destination mode of the IPI.
enum destination_mode_t {
    // The destination field is an APIC ID. That is only one processor will
    // receive the interrupt.
    PHYSICAL = 0,
    // The destination field is a set of processors. Unsupported for now.
    LOGICAL = 1,
} __attribute__((packed));

// Trigger mode of the interrupt.
enum trigger_mode_t {
    // Edge sensitive.
    EDGE = 0,
    // Level sensitive.
    LEVEL = 1,
} __attribute__((packed));

// Describes shorthand for the interrupt destination (eg. target processor(s)).
enum destination_shorthand_t {
    // No shorthand. The interrupt will be sent to the processor having the APIC
    // ID as in the destination field.
    NONE = 0,
    // Send interrupt to self. Destination field is unused.
    SELF = 1,
    // Send interrupt to all the cpus in the system, including the cpu sending
    // the interrupt. Destination field is unused.
    ALL_INCL_SELF = 2,
    // Send interrupt to all the cpus in the system excepted the cpu sending the
    // interrupt. Destination field is unused.
    ALL_EXCL_SELF = 3,
} __attribute__((packed));

// Layout of the ICR if we assume both double words are continuous in the LAPIC
// register area. This structure makes it easy to program the ICR but does not
// reflect the physical layout of it (in practice the ICR is split into to
// lapic_reg_32_t with a hole in between).
struct icr {
    union {
        struct {
            // The ICR is made up of two double words that need to be written in
            // the order: high then low.
            // This struct provide an easy way to access those double words.
            uint32_t low;
            uint32_t high;
        } __attribute__((packed));

        // The real layout of the ICR.
        struct {
            // The vector of the IPI. This is only useful for NORMAL delivery
            // mode (see below).
            uint8_t vector : 8;

            // The delivery mode is the IPI message type.
            enum delivery_mode_t delivery_mode : 3;

            // Changes the meaning of the destination field defined below.
            enum destination_mode_t destination_mode : 1;
            uint8_t : 2;

            // The level of the interrupt, this is only useful for INIT IPIs.
            // All other delivery modes must use 1.
            uint8_t level : 1;

            // Set the trigger mode for the IPI. This is only used for INIT
            // IPIs with level = 0 (de-assert) and is ignored for all other
            // types.
            enum trigger_mode_t trigger_mode : 1;
            uint8_t : 2;

            // Provides the ability to broadcast an IPI. If not NONE, the
            // destination field is ignored (since the IPI is either sent to
            // self or broadcasted).
            enum destination_shorthand_t destination_shorthand : 2;
            uint64_t : 36;

            // The destination of the IPI. Interpretation differs depending on
            // delivery and destination mode and destination shorthand.
            uint8_t destination : 8;
        } __attribute__((packed));
    } __attribute__((packed));
    uint8_t padding[24];
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct icr) == 32, "");

#define RESERVED    \
    uint64_t : 64;  \
    uint64_t : 64;

struct lapic {
    RESERVED
    RESERVED

    lapic_reg_32_t local_apic_id;
    lapic_reg_32_t const local_apic_version;

    RESERVED
    RESERVED
    RESERVED
    RESERVED

    lapic_reg_32_t task_priority;
    lapic_reg_32_t const arbitration_priority;
    lapic_reg_32_t const processor_priority;
    lapic_reg_32_t eoi;
    lapic_reg_32_t const remote_read;
    lapic_reg_32_t logical_destination;
    lapic_reg_32_t destination_format;
    lapic_reg_32_t spurious_interrupt_vector;

    lapic_reg_256_t const in_service;
    lapic_reg_256_t const trigger_mode;
    lapic_reg_256_t const interrupt_request;

    lapic_reg_32_t const error_status;

    RESERVED
    RESERVED
    RESERVED
    RESERVED
    RESERVED
    RESERVED

    lapic_reg_32_t lvt_correctedd_machine_check_interrupt;
    lapic_reg_64_t interrupt_command;
    lapic_reg_32_t lvt_timer;
    lapic_reg_32_t lvt_thermal_sensor;
    lapic_reg_32_t lvt_performance_monitoring_counters;
    lapic_reg_32_t lvt_lint0;
    lapic_reg_32_t lvt_lint1;
    lapic_reg_32_t lvt_error;
    lapic_reg_32_t initial_count;
    lapic_reg_32_t const current_count;

    RESERVED
    RESERVED
    RESERVED
    RESERVED

    lapic_reg_32_t divide_configuration;
    RESERVED
} __attribute__((packed));
// Make sure the registers are at the right offsets.
STATIC_ASSERT(offsetof(struct lapic, local_apic_id) == 0x20, "");
STATIC_ASSERT(offsetof(struct lapic, local_apic_version) == 0x30, "");
STATIC_ASSERT(offsetof(struct lapic, task_priority) == 0x80, "");
STATIC_ASSERT(offsetof(struct lapic, arbitration_priority) == 0x90, "");
STATIC_ASSERT(offsetof(struct lapic, processor_priority) == 0xA0, "");
STATIC_ASSERT(offsetof(struct lapic, eoi) == 0xB0, "");
STATIC_ASSERT(offsetof(struct lapic, remote_read) == 0xC0, "");
STATIC_ASSERT(offsetof(struct lapic, logical_destination) == 0xD0, "");
STATIC_ASSERT(offsetof(struct lapic, destination_format) == 0xE0, "");
STATIC_ASSERT(offsetof(struct lapic, spurious_interrupt_vector) == 0xF0, "");
STATIC_ASSERT(offsetof(struct lapic, in_service) == 0x100, "");
STATIC_ASSERT(offsetof(struct lapic, trigger_mode) == 0x180, "");
STATIC_ASSERT(offsetof(struct lapic, interrupt_request) == 0x200, "");
STATIC_ASSERT(offsetof(struct lapic, error_status) == 0x280, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_correctedd_machine_check_interrupt)
        == 0x2F0, "");
STATIC_ASSERT(offsetof(struct lapic, interrupt_command) == 0x300, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_timer) == 0x320, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_thermal_sensor) == 0x330, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_performance_monitoring_counters)
        == 0x340, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_lint0) == 0x350, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_lint1) == 0x360, "");
STATIC_ASSERT(offsetof(struct lapic, lvt_error) == 0x370, "");
STATIC_ASSERT(offsetof(struct lapic, initial_count) == 0x380, "");
STATIC_ASSERT(offsetof(struct lapic, current_count) == 0x390, "");
STATIC_ASSERT(offsetof(struct lapic, divide_configuration) == 0x3E0, "");
