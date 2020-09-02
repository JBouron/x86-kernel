#include <ioapic.h>
#include <types.h>
#include <acpi.h>
#include <debug.h>
#include <cpu.h>
#include <paging.h>
#include <memory.h>
#include <spinlock.h>

// The register addresses of the IO APIC.
#define IOAPICID    0
#define IOAPICVER   1
#define IOAPICARB   2
#define IOREDTBL(n) (0x10 + (n) * 2)

// This is the memory mapped portion of the IO APIC.
struct io_apic {
    // The ioregsel selects a register to read from or write to in the IO APIC.
    struct {
        // The address of the register to read from/write to.
        uint8_t reg_addr : 8;
        // Reserved.
        uint32_t : 24;
    } ioregsel;
    // Reserved.
    uint64_t : 64;
    // Reserved.
    uint32_t : 32;
    // Once ioregsel.reg_addr is set this field can be either read or written to
    // in order to perform a read or a write from/to the register of the IO
    // APIC.
    uint32_t iowin;
} __attribute__((packed));

// There are usually less IOAPICs than cores on a system (this kernel currently
// supports a single IOAPIC anyway). Therefore a lock is required to avoid race
// condition if two cpus try to write IOAPIC registers concurrently.
DECLARE_SPINLOCK(IOAPIC_LOCK);

// The virtual address of the current IO APIC. For now we assume only one.
struct io_apic volatile * IO_APIC = NULL;

// Redirection entry:
// The next type definitions are related to redirection entries.

// Indicate the delivery mode of an interrupt from the IO APIC.
enum delivery_mode_t {
    // Deliver to processor(s) in destination field.
    NORMAL = 0,
    // Deliver to the processor with lowest priority.
    LOW_PRIO = 1,
    // Deliver a SMI. Must be EDGE level mode and vector = 0.
    SYSTEM_MANAGEMENT_INTERRUPT = 2,
    // Deliver an NMI. Must be EDGE level.
    NON_MASKABLE = 4,
    // Deliver to processor(s) in destination field with an INIT interrupt. Must
    // be used with EDGE level mode.
    INIT = 5,
    // Deliver to processor(s) in destination field with an INIT interrupt. Must
    // be used with EDGE level mode.
    EXTERNAL = 7,
} __attribute__((packed));

// Indicate how the destination field of a redirection entry should be
// interpreted.
enum destination_mode_t {
    // The destination field is an APIC ID. That is only one processor will
    // receive the interrupt.
    PHYSICAL = 0,
    // The destination field is a set of processors. Unsupported for now.
    LOGICAL = 1,
} __attribute__((packed));

// Specifies the polarity of the interrupt.
enum polarity_t {
    // Active on high.
    HIGH = 0,
    // Active on low.
    LOW = 1,
} __attribute__((packed));

// Trigger mode of the interrupt.
enum trigger_mode_t {
    // Edge sensitive.
    EDGE = 0,
    // Level sensitive.
    LEVEL = 1,
} __attribute__((packed));

// The redirection entry format.
struct redirection_entry {
    union {
        struct {
            // Each redirection entry is 64 bits long. However, reading and
            // writing from the IO APIC is done by size of 32 bits. Hence this
            // union provides an easy way to access the lower or higher 32 bits
            // of the entry.
            uint32_t low : 32;
            uint32_t high : 32;
        };
        // This struct is the real format of an entry.
        struct {
            // The vector to redirect the interrupt to. That is, if this entry
            // is the first entry (index = 0) then all interrupt 0 will be
            // redirected with a vector of `vector` instead.
            uint8_t vector : 8;
            // The delivery mode of the interrupt.
            enum delivery_mode_t delivery_mode : 3;
            // The destination mode of the interrupt.
            enum destination_mode_t destination_mode : 1;
            // [CONST] If set, indicates that the interrupt is waiting for the
            // APIC before being triggered.
            bool waiting_for_apic : 1;
            // The polarity of the interrupt.
            enum polarity_t polarity : 1;
            // Reserved.
            bool : 1;
            // The trigger mode of the interrupt.
            enum trigger_mode_t trigger_mode : 1;
            // If set, any subsequent interrupt of this vector will be ignored.
            bool masked : 1;
            // Reserved.
            uint64_t : 39;
            struct {
                // The APIC ID of the processor to route the interrupt to.
                uint8_t dest : 4;
                // Reserved.
                uint8_t : 4;
            };
        };
    };
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct redirection_entry) == 8, "");

// Check if a redirection entry is valid.
// @param entry: The entry to test.
// @return: true if entry is a valid redirection entry, false otherwise.
static bool redirection_entry_is_valid(
    struct redirection_entry const * const entry) {
    // Some of the delivery mode are only compatible with some trigger mode (or
    // vector).
    switch (entry->delivery_mode) {
        case SYSTEM_MANAGEMENT_INTERRUPT:
            // Must be EDGE level mode and vector = 0.
            return entry->trigger_mode == EDGE && entry->vector == 0;
        case NON_MASKABLE:
            // Must be EDGE level.
            return entry->trigger_mode == EDGE;
        case INIT:
            // Must be EDGE level.
            return entry->trigger_mode == EDGE;
        case EXTERNAL:
            // Must be EDGE level.
            return entry->trigger_mode == EDGE;
        default:
            return true;
    }
    // Unreachable.
}

// Read a register from the IO APIC.
// @param reg: The address of the register to be read.
// @return: The current value of the IO APIC register.
static uint32_t read_register(uint8_t const reg) {
    IO_APIC->ioregsel.reg_addr = reg;
    return IO_APIC->iowin;
}

// Write one of the IO APIC's registers.
// @param reg: The address of the register to be written.
// @param value: The value to write into the register.
static void write_register(uint8_t const reg, uint32_t const value) {
    IO_APIC->ioregsel.reg_addr = reg;
    IO_APIC->iowin = value;
}

// This struct describes the layout of the value returned when reading the
// register IOAPICVER.
struct ioapicver {
    union {
        uint32_t raw;
        struct {
            // The version of the APIC. Should _always_ be 0x11.
            uint8_t const apic_version : 8;
            // Reserved.
            uint8_t : 8;
            // The entry number of the highest entry in the IO redirection
            // table.
            uint8_t const max_redirections : 8;
            // Reserved.
            uint8_t : 8;
        };
    };
} __attribute__((packed));

// Get the maximum number of redirections from the IO APIC.
// @return: The maximum number of redirections, that is the number of entries in
// the redirection table. Note that this is different from the max_redirection
// in the IOAPICVER register which indicates the entry number of the highest
// entry.
static uint8_t get_max_redirections(void) {
    struct ioapicver const ioapicver = {
        .raw = read_register(IOAPICVER),
    };
    return ioapicver.max_redirections + 1;
}

// Read a redirection entry from the IO APIC.
// @param index: The index of the entry to read in the redirection table.
// @param dest [OUT]: Pointer to read the entry into.
static void read_redirection(uint8_t const index,
    struct redirection_entry * const dest) {
    uint8_t const reg = IOREDTBL(index);
    dest->low = read_register(reg);
    dest->high = read_register(reg + 1);
}

// Write a redirection entry in the redirection table.
// @param index: The index in the redirection table.
// @param entry: The entry to be written.
static void write_redirection(uint8_t const index,
                              struct redirection_entry const * const entry) {
    ASSERT(redirection_entry_is_valid(entry));
    uint8_t const reg = IOREDTBL(index);

    // According to the specification, to modify a field of a register, one must
    // read the entire register, change the field and write it back. Since the
    // redirection entries have reserved fields, we need to proceed in this
    // manner.
    struct redirection_entry curr_entry;
    read_redirection(index, &curr_entry);

    // Mutate the fields of the current entry.
    curr_entry.vector = entry->vector;
    curr_entry.delivery_mode = entry->delivery_mode;
    curr_entry.destination_mode = entry->destination_mode;
    curr_entry.polarity = entry->polarity;
    curr_entry.trigger_mode = entry->trigger_mode;
    curr_entry.masked = entry->masked;
    curr_entry.waiting_for_apic = 0;
    curr_entry.dest = entry->dest;

    // Write back the entry.
    write_register(reg, curr_entry.low);
    write_register(reg + 1, curr_entry.high);
}

// Get the APIC ID of the processor that should receive interrupts of a given
// vector.
// @param vector: The vector.
// @return: The APIC ID of the processor to use in the destination field.
static uint8_t compute_destination_for_interrupt(uint8_t const vector) {
    // TODO: Try to do some load balancing once all cores are up.
    ASSERT(vector >= 32);
    return cpu_apic_id();
}

void init_ioapic(void) {
    // Since this function is called by the BSP before waking up the APs we can
    // avoid acquiring the IOAPIC_LOCK.

    IO_APIC = acpi_get_ioapic_addr();
    LOG("IO APIC at %p\n", IO_APIC);

    // Map the IO APIC to virtual memory using identity mapping. Make sure to
    // use write through and disable cache for the page.
    uint32_t const map_flags = VM_WRITE | VM_WRITE_THROUGH | VM_CACHE_DISABLE;
    void * const io_apic_addr = (void*)IO_APIC;
    if (!paging_map(io_apic_addr, io_apic_addr, sizeof(*IO_APIC), map_flags)) {
        PANIC("Cannot map IOAPIC to virtual memory\n");
    }

    LOG("IOAPICID   = %x\n", read_register(IOAPICID));
    LOG("IOAPICVER  = %x\n", read_register(IOAPICVER));
    LOG("IOAPICARB  = %x\n", read_register(IOAPICARB));
    LOG("Max redirections = %u\n", get_max_redirections());
}

void redirect_isa_interrupt(uint8_t const isa_vector,
                            uint8_t const new_vector) {
    ASSERT(isa_vector <= 15);
    // Interrupt vector 0-31 are reserved. Make sure the requested vector is not
    // one of them.
    ASSERT(31 < new_vector);

    struct redirection_entry redir;
    // Make sure to zero the struct before writing it. Failure to do so might
    // write some value in reserved bit which leads to undefined behavior.
    memzero(&redir, sizeof(redir));

    // Use sane defaults for the redirection.
    redir.delivery_mode = NORMAL;
    redir.destination_mode = PHYSICAL;
    redir.polarity = HIGH;
    redir.trigger_mode = EDGE;
    redir.masked = 0;
    redir.waiting_for_apic = 0;

    // Compute which CPU should handle this interrupt.
    redir.dest = compute_destination_for_interrupt(new_vector);

    // Write the redirection entry in the IO APIC.
    redir.vector = new_vector;

    // The ISA vector might have been mapped to another vector on the IO APIC.
    uint8_t const mappedisa = acpi_get_isa_interrupt_vector_mapping(isa_vector);

    spinlock_lock(&IOAPIC_LOCK);
    write_redirection(mappedisa, &redir);
    spinlock_unlock(&IOAPIC_LOCK);
}

void remove_redirection_for_isa_interrupt(uint8_t const isa_vector) {
    ASSERT(isa_vector <= 15);
    // Get the index of the ISA vector in the redirection table.
    uint8_t const mappedisa = acpi_get_isa_interrupt_vector_mapping(isa_vector);

    // Read the current redirection entry for this ISA vector.
    struct redirection_entry curr_entry;

    // Atomically mask the interrupt.
    spinlock_lock(&IOAPIC_LOCK);
    read_redirection(mappedisa, &curr_entry);

    // Mask the interrupt.
    curr_entry.masked = true;

    // Write-back the entry.
    write_redirection(mappedisa, &curr_entry);
    spinlock_unlock(&IOAPIC_LOCK);
}

#include <ioapic.test>
