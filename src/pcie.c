#include <pcie.h>
#include <types.h>
#include <debug.h>
#include <acpi.h>
#include <paging.h>
#include <pci_class_code.h>

// Layout of the MCFG table given from the ACPI.
struct mcfg_table {
    // Base address of the Configuration space mapped to memory.
    uint64_t const base_addr;
    // Segment id.
    uint16_t const pci_group_number;    
    // Smallest bus number.
    uint8_t const start_pci_bus_number;
    // Highest bus number.
    uint8_t const end_pci_bus_number;
    uint32_t : 32;
} __attribute__((packed));

// Pointer to MCFG table parsed by the ACPI sub-system.
struct mcfg_table const * MCFG_TABLE = NULL;

typedef uint16_t bus_t;
typedef uint8_t dev_t;
typedef uint8_t func_t;

// Common header fields for all PCIe configuration spaces.
struct pcie_header {
    // ID of the Vendor for this device. Unique per vendor. 0xFFFF indicates and
    // invalid value.
    uint16_t const vendor_id;
    // ID of the device.
    uint16_t const device_id;
    // Command register, provides control over a device's ability to generate
    // and respond to PCI cycles.
    union {
        // If writing 0 to this register, the device is disconnected from the
        // PCI bus for all accesses except Configuration Space access.
        uint16_t value;
        struct {
            // If set, device can respond to I/O Space accesses.
            uint8_t io_space : 1;
            // If set, device can respond to Memory Space accesses.
            uint8_t memory_space : 1;
            // If set, device can behave as bus master, otherwise it cannot
            // generate PCI accesses.
            uint8_t bus_master : 1;
            uint8_t : 3;
            // If set to 1 the device will take its normal action when a parity
            // error is detected; otherwise, when an error is detected, the
            // device will set bit 15 of the Status register (Detected Parity
            // Error Status Bit), but will not assert the PERR# (Parity Error)
            // pin and will continue operation as normal. 
            uint8_t parity_error_response : 1;
            uint8_t : 1;
            // If set to 1 the SERR# driver is enabled; otherwise, the driver is
            // disabled. 
            uint8_t serr_enable : 1;
            uint8_t : 1;
            // If set to 1 the assertion of the devices INTx# signal is
            // disabled; otherwise, assertion of the signal is enabled.
            uint8_t interrupt_disable : 1;
            uint8_t : 5;
        } __attribute__((packed));
    } __attribute__((packed)) command;

    union {
        // A register used to record status information for PCI bus related
        // events.
        uint16_t const value;
        struct {
            uint8_t : 2;
            // Represents the state of the device's INTx# signal. If set to 1
            // and bit 10 of the Command register (Interrupt Disable bit) is set
            // to 0 the signal will be asserted; otherwise, the signal will be
            // ignored.
            uint8_t const interrupt_status : 1;
            uint8_t : 4;
            // This bit is only set when the following conditions are met. The
            // bus agent asserted PERR# on a read or observed an assertion of
            // PERR# on a write, the agent setting the bit acted as the bus
            // master for the operation in which the error occurred, and bit 6
            // of the Command register (Parity Error Response bit) is set to 1.
            uint8_t const master_data_parity_error : 1;
            uint8_t : 2;
            // This bit will be set to 1 whenever a target device terminates a
            // transaction with Target-Abort.
            uint8_t const signalled_target_abort : 1;
            // This bit will be set to 1, by a master device, whenever its
            // transaction is terminated with Target-Abort.
            uint8_t const received_target_abort : 1;
            // This bit will be set to 1, by a master device, whenever its
            // transaction (except for Special Cycle transactions) is terminated
            // with Master-Abort.
            uint8_t const recieved_master_abort : 1;
            // This bit will be set to 1 whenever the device asserts SERR#.
            uint8_t const signaled_system_error : 1;
            // This bit will be set to 1 whenever the device detects a parity
            // error, even if parity error handling is disabled.
            uint8_t const detected_parity_error : 1;
        } __attribute__((packed));
    } __attribute__((packed)) status;

    // Revision of the device, allocated by vendor.
    uint8_t const revision_id;
    // Programming Interface Byte. A read only register that specifies a
    // register-level programming interface the device has, if any.
    uint8_t const prog_intf;
    // Specifies the specific function the device performs.
    uint8_t const subclass;
    // Specifies the type of function the device performs.
    uint8_t const class_code;
    uint16_t : 16;
    struct {
        // Define the type of the header and the layout of the rest of the
        // enclosing struct starting at offset 0x10 (after built_in_self_test).
        uint8_t const header_type : 7;
        // If set the device has multiple functions, otherwise this is a single
        // function device.
        uint8_t const multi_funcs : 1;
    } __attribute__((packed));

    // Built-in-Self-Test (BIST) information.
    union {
        uint8_t value;
        struct {
            // 0 if BIST was successful.
            uint8_t const completion_code : 4;
            uint8_t : 2;
            // When set to 1, start the BIST. This gets reset upon BIST
            // completion. If BIST does not complete after 2s the OS should
            // consider this device failing.
            uint8_t start : 1;
            // Indicate if this device is capable of running a BIST.
            uint8_t const capable : 1;
        } __attribute__((packed));
    } __attribute__((packed)) bist;
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct pcie_header) == 0x10, "");

// Base Address Register (BAR) value. Indicate memory addresse or IO port
// addresse used by the device.
struct pcie_bar {
    union {
        // Raw value of the BAR.
        uint32_t const value;

        // If set, this BAR descrive IO space address. Otherwise describe Memory
        // space address.
        // For some reason this needs to be within the union for GCC to generate
        // a struct pcie_bar of exactly 4 bytes.
        uint8_t const is_io_space : 1;

        // IO space address, (e.g. if is_io_space is set).
        struct {
            uint8_t : 2;
            // 4-bytes aligned base address.
            uint32_t const io_base_addr : 30;
        } __attribute__((packed));

        // Memory space address, (e.g. if is_io_space is set).
        struct {
            uint8_t : 1;
            // Indicate size of base register and where in memory it can be
            // mapped:
            //  - 0x0: base register is 32-bit wide, can map in 32-bit addr
            //  space.
            //  - 0x2: 64-bit wide, map in 64-bit address space. In this case
            //  this BAR contains the lower bits of the address, while the next
            //  one contains the higher bits.
            //  - 0x1: reserved.
            uint8_t const type : 2;
            // Indicate if the region does not have read side effects (reading
            // from that memory range doesn't change any state).
            uint8_t const prefetchable : 1;
            // 16-bytes aligned base address.
            uint32_t const mem_base_addr : 28;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct pcie_bar) == 4, "");

// Layout of the configuration space for header_type == 0x0.
struct pcie_device {
    // Common header.
    struct pcie_header header;
    // Base Address Registers (BARs).
    struct pcie_bar const bar[6];
    // Points to the Card Information Structure and is used by devices that
    // share silicon between CardBus and PCI.
    uint32_t const cardbus_cis_ptr;
    uint16_t const subsystem_vendor_id;
    uint16_t const subsystem_id;
    uint32_t const expansion_rom_base_addr;
    uint8_t const capabilities_ptr;
    uint64_t : 56;
    // Specifies which input of the system interrupt controllers the device's
    // interrupt pin is connected to and is implemented by any device that makes
    // use of an interrupt pin. 0xFF indicates not connection.
    uint8_t const interrupt_line;
    // Specifies which interrupt pin the device uses. Where a value of 0x01 is
    // INTA#, 0x02 is INTB#, 0x03 is INTC#, 0x04 is INTD#, and 0x00 means the
    // device does not use an interrupt pin.
    uint8_t const interrupt_pin;
    uint16_t : 16;
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct pcie_device) == 0x40, "");

// Read the PCIe header structure for a given device.
// @param bus: The bus of the device.
// @param dev: The id of the device on the bus.
// @param func: The function of the device.
// @return: The struct pcie_header associated with this PCIe device.
static struct pcie_header *read_pcie_dev(bus_t const bus,
                                         dev_t const dev,
                                         func_t const func) {
    if ((uint32_t)MCFG_TABLE->base_addr != MCFG_TABLE->base_addr) {
        PANIC("Base address of MCFG table is outside of 32-bit address space");
    }
    return (void*)(uint32_t)MCFG_TABLE->base_addr +
        (((bus - MCFG_TABLE->start_pci_bus_number) << 20) |
         (dev << 15) |
         (func << 12));
}

// Check for the existence of a PCIe device.
// @param bus: The bus of the device.
// @param dev: The id of the device.
// @param func: The function of the device.
static void check_device(bus_t const bus, dev_t const dev, func_t const func) {
    struct pcie_header const * const header = read_pcie_dev(bus, dev, func);

    // Map the device to kernel memory. FIXME: We should avoid ID-mapping here
    // and simply map to any available kernel address.
    ASSERT(paging_map(header, header, PAGE_SIZE, func));

    if (header->vendor_id == 0xFFFF) {
        // Invalid vendor ID means that there are no PCIe device.
        paging_unmap(header, PAGE_SIZE);
        return;
    }

    LOG("  Bus %d, Dev %d, Func %d: ", bus, dev, func);
    pci_pretty_print_dev(header->class_code,
                         header->subclass,
                         header->prog_intf);
    LOG("\n");
    LOG("    VendorID:DeviceID = %x:%x\n    Class:Subclass:progif = %x:%x:%x\n",
        header->vendor_id,
        header->device_id,
        header->class_code,
        header->subclass,
        header->prog_intf);

    // TODO: For now devices with header type != 0x0 are not supported.
    ASSERT(header->header_type == 0x0);
    struct pcie_device const * const d = (void*)header;

    if (d->interrupt_line && d->interrupt_pin) {
        // Qemu seems to skip the IRQ/PIN pair when the irq or pin is 0. Do the
        // same.
        LOG("    IRQ %d, pin %c\n", d->interrupt_line, 'A'+d->interrupt_pin-1);
    }
    for (uint8_t i = 0; i < 6; ++i) {
        if (!d->bar[i].value) {
            continue;
        }
        LOG("    BAR%d: ", i);
        if (d->bar[i].is_io_space) {
            LOG("I/O at %x\n", d->bar[i].io_base_addr << 2);
        } else if (!d->bar[i].type) {
            LOG("32 bit memory at %x\n", d->bar[i].mem_base_addr << 4);
        } else {
            // This kernel does not support 64-bit addresses.
            PANIC("Unsupported BAR type: %d\n", d->bar[i].type);
        }
    }

    // For multi-function device, recurse and enumerate the sub-functions.
    bool const recurse_on_funcs = header->multi_funcs && !func;

    paging_unmap(header, PAGE_SIZE);
    if (recurse_on_funcs) {
        for (func_t f = 1; f < 8; ++f) {
            check_device(bus, dev, f);
        }
    }
}

// Enumerate all PCIe devices on the system and pretty print them.
static void enumerate_pcie_devices(void) {
    for (bus_t bus = 0; bus < 256; ++bus) {
        for (dev_t dev = 0; dev < 32; ++dev) {
            check_device(bus, dev, 0);
        }
    }
}

void init_pcie(void) {
    MCFG_TABLE = get_mcfg_table();
    enumerate_pcie_devices();
}
