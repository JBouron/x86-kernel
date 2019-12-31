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

#define RESERVED    \
    uint64_t : 64;  \
    uint64_t : 64;

struct lapic_t {
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
STATIC_ASSERT(offsetof(struct lapic_t, local_apic_id) == 0x20, "");
STATIC_ASSERT(offsetof(struct lapic_t, local_apic_version) == 0x30, "");
STATIC_ASSERT(offsetof(struct lapic_t, task_priority) == 0x80, "");
STATIC_ASSERT(offsetof(struct lapic_t, arbitration_priority) == 0x90, "");
STATIC_ASSERT(offsetof(struct lapic_t, processor_priority) == 0xA0, "");
STATIC_ASSERT(offsetof(struct lapic_t, eoi) == 0xB0, "");
STATIC_ASSERT(offsetof(struct lapic_t, remote_read) == 0xC0, "");
STATIC_ASSERT(offsetof(struct lapic_t, logical_destination) == 0xD0, "");
STATIC_ASSERT(offsetof(struct lapic_t, destination_format) == 0xE0, "");
STATIC_ASSERT(offsetof(struct lapic_t, spurious_interrupt_vector) == 0xF0, "");
STATIC_ASSERT(offsetof(struct lapic_t, in_service) == 0x100, "");
STATIC_ASSERT(offsetof(struct lapic_t, trigger_mode) == 0x180, "");
STATIC_ASSERT(offsetof(struct lapic_t, interrupt_request) == 0x200, "");
STATIC_ASSERT(offsetof(struct lapic_t, error_status) == 0x280, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_correctedd_machine_check_interrupt)
        == 0x2F0, "");
STATIC_ASSERT(offsetof(struct lapic_t, interrupt_command) == 0x300, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_timer) == 0x320, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_thermal_sensor) == 0x330, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_performance_monitoring_counters)
        == 0x340, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_lint0) == 0x350, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_lint1) == 0x360, "");
STATIC_ASSERT(offsetof(struct lapic_t, lvt_error) == 0x370, "");
STATIC_ASSERT(offsetof(struct lapic_t, initial_count) == 0x380, "");
STATIC_ASSERT(offsetof(struct lapic_t, current_count) == 0x390, "");
STATIC_ASSERT(offsetof(struct lapic_t, divide_configuration) == 0x3E0, "");
