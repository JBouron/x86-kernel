#pragma once

// This file contains the mapping (class, subclass, progif) to name/description
// used to pretty pring the PCI device enumeration.
// See helper function defined below the arrays.

// Mapping from (class, subclass, progif) -> name.
struct _csp_map {
	uint8_t const class;
	uint8_t const subclass;
	uint8_t const progif;
	char const * const name;
};

// Maps all known tuples (Class, subclass, progif) to description/name.
static struct _csp_map const CLASS_SUBCLASS_PROGIF_MAP[] = {
	{0x01, 0x01, 0x00, "ISA Compatibility mode-only controller"},
	{0x01, 0x01, 0x05, "PCI native mode-only controller"},
	{0x01, 0x01, 0x0A, "ISA Compatibility mode controller, supports both "
        "channels switched to PCI native mode"},
	{0x01, 0x01, 0x0F, "PCI native mode controller, supports both channels "
        "switched to ISA compatibility mode"},
	{0x01, 0x01, 0x80, "ISA Compatibility mode-only controller, supports bus "
        "mastering"},
	{0x01, 0x01, 0x85, "PCI native mode-only controller, supports bus "
        "mastering"},
	{0x01, 0x01, 0x8A, "ISA Compatibility mode controller, supports both "
        "channels switched to PCI native mode, support bus mastering"},
	{0x01, 0x01, 0x8F, "PCI native mode controller, supports both channels "
        "switched to ISA compatibility mode, supports bus mastering"},
	{0x01, 0x05, 0x20, "Single DMA"},
	{0x01, 0x05, 0x30, "Chained DMA"},
	{0x01, 0x06, 0x00, "Vendor Specific Interface"},
	{0x01, 0x06, 0x01, "AHCI 1.0"},
	{0x01, 0x06, 0x02, "Serial Storage Bus"},
	{0x01, 0x07, 0x00, "SAS"},
	{0x01, 0x07, 0x01, "Serial Storage Bus"},
	{0x01, 0x08, 0x01, "NVMHCI"},
	{0x01, 0x08, 0x02, "NVM Express"},
	{0x03, 0x00, 0x00, "VGA Controller"},
	{0x03, 0x00, 0x01, "8514-Compatible Controller"},
	{0x06, 0x04, 0x00, "Normal Decode"},
	{0x06, 0x04, 0x01, "Subtractive Decode"},
	{0x06, 0x08, 0x00, "Transparent Mode"},
	{0x06, 0x08, 0x01, "Endpoint Mode"},
	{0x06, 0x09, 0x40, "Semi-Transparent, Primary bus towards host CPU"},
	{0x06, 0x09, 0x80, "Semi-Transparent, Secondary bus towards host CPU"},
	{0x07, 0x00, 0x00, "8250-Compatible (Generic XT)"},
	{0x07, 0x00, 0x01, "16450-Compatible"},
	{0x07, 0x00, 0x02, "16550-Compatible"},
	{0x07, 0x00, 0x03, "16650-Compatible"},
	{0x07, 0x00, 0x04, "16750-Compatible"},
	{0x07, 0x00, 0x05, "16850-Compatible"},
	{0x07, 0x00, 0x06, "16950-Compatible"},
	{0x07, 0x01, 0x00, "Standard Parallel Port"},
	{0x07, 0x01, 0x01, "Bi-Directional Parallel Port"},
	{0x07, 0x01, 0x02, "ECP 1.X Compliant Parallel Port"},
	{0x07, 0x01, 0x03, "IEEE 1284 Controller"},
	{0x07, 0x01, 0xFE, "IEEE 1284 Target Device"},
	{0x07, 0x03, 0x00, "Generic Modem"},
	{0x07, 0x03, 0x01, "Hayes 16450-Compatible Interface"},
	{0x07, 0x03, 0x02, "Hayes 16550-Compatible Interface"},
	{0x07, 0x03, 0x03, "Hayes 16650-Compatible Interface"},
	{0x07, 0x03, 0x04, "Hayes 16750-Compatible Interface"},
	{0x08, 0x00, 0x00, "Generic 8259-Compatible"},
	{0x08, 0x00, 0x01, "ISA-Compatible"},
	{0x08, 0x00, 0x02, "EISA-Compatible"},
	{0x08, 0x00, 0x10, "I/O APIC Interrupt Controller"},
	{0x08, 0x00, 0x20, "I/O(x) APIC Interrupt Controller"},
	{0x08, 0x01, 0x00, "Generic 8237-Compatible"},
	{0x08, 0x01, 0x01, "ISA-Compatible"},
	{0x08, 0x01, 0x02, "EISA-Compatible"},
	{0x08, 0x02, 0x00, "Generic 8254-Compatible"},
	{0x08, 0x02, 0x01, "ISA-Compatible"},
	{0x08, 0x02, 0x02, "EISA-Compatible"},
	{0x08, 0x02, 0x03, "HPET"},
	{0x08, 0x03, 0x00, "Generic RTC"},
	{0x08, 0x03, 0x01, "ISA-Compatible"},
	{0x09, 0x04, 0x00, "Generic"},
	{0x09, 0x04, 0x10, "Extended"},
	{0x0C, 0x00, 0x00, "Generic"},
	{0x0C, 0x00, 0x10, "OHCI"},
	{0x0C, 0x03, 0x00, "UHCI Controller"},
	{0x0C, 0x03, 0x10, "OHCI Controller"},
	{0x0C, 0x03, 0x20, "EHCI (USB2) Controller"},
	{0x0C, 0x03, 0x30, "XHCI (USB3) Controller"},
	{0x0C, 0x03, 0x80, "Unspecified"},
	{0x0C, 0x03, 0xFE, "USB Device (Not a host controller)"},
	{0x0C, 0x07, 0x00, "SMIC"},
	{0x0C, 0x07, 0x01, "Keyboard Controller Style"},
	{0x0C, 0x07, 0x02, "Block Transfer"},
	{0x00, 0x00, 0x00, NULL},
};

// Mapping from (class, subclass) -> name.
struct _cs_map {
	uint8_t const class;
	uint8_t const subclass;
	char const * const name;
};

// Maps all known pairs (Class, subclass) to description/name.
static struct _cs_map CLASS_SUBCLASS_MAP[] = {
	{0x00, 0x00, "Non-VGA-Compatible devices"},
	{0x00, 0x01, "VGA-Compatible Device"},
	{0x01, 0x00, "SCSI Bus Controller"},
	{0x01, 0x01, "IDE Controller"},
	{0x01, 0x02, "Floppy Disk Controller"},
	{0x01, 0x03, "IPI Bus Controller"},
	{0x01, 0x04, "RAID Controller"},
	{0x01, 0x05, "ATA Controller"},
	{0x01, 0x06, "Serial ATA"},
	{0x01, 0x07, "Serial Attached SCSI"},
	{0x01, 0x08, "Non-Volatile Memory Controller"},
	{0x01, 0x80, "Other"},
	{0x02, 0x00, "Ethernet Controller"},
	{0x02, 0x01, "Token Ring Controller"},
	{0x02, 0x02, "FDDI Controller"},
	{0x02, 0x03, "ATM Controller"},
	{0x02, 0x04, "ISDN Controller"},
	{0x02, 0x05, "WorldFip Controller"},
	{0x02, 0x06, "PICMG 2.14 Multi Computing"},
	{0x02, 0x07, "Infiniband Controller"},
	{0x02, 0x08, "Fabric Controller"},
	{0x02, 0x80, "Other"},
	{0x03, 0x00, "VGA Compatible Controller"},
	{0x03, 0x01, "XGA Controller"},
	{0x03, 0x02, "3D Controller (Not VGA-Compatible)"},
	{0x03, 0x80, "Other"},
	{0x04, 0x00, "Multimedia Video Controller"},
	{0x04, 0x01, "Multimedia Audio Controller"},
	{0x04, 0x02, "Computer Telephony Device"},
	{0x04, 0x03, "Audio Device"},
	{0x04, 0x80, "Other"},
	{0x05, 0x00, "RAM Controller"},
	{0x05, 0x01, "Flash Controller"},
	{0x05, 0x80, "Other"},
	{0x06, 0x00, "Host Bridge"},
	{0x06, 0x01, "ISA Bridge"},
	{0x06, 0x02, "EISA Bridge"},
	{0x06, 0x03, "MCA Bridge"},
	{0x06, 0x04, "PCI-to-PCI Bridge"},
	{0x06, 0x05, "PCMCIA Bridge"},
	{0x06, 0x06, "NuBus Bridge"},
	{0x06, 0x07, "CardBus Bridge"},
	{0x06, 0x08, "RACEway Bridge"},
	{0x06, 0x09, "PCI-to-PCI Bridge"},
	{0x06, 0x0A, "InfiniBand-to-PCI Host Bridge"},
	{0x06, 0x80, "Other"},
	{0x07, 0x00, "Serial Controller"},
	{0x07, 0x01, "Parallel Controller"},
	{0x07, 0x02, "Multiport Serial Controller"},
	{0x07, 0x03, "Modem"},
	{0x07, 0x04, "IEEE 488.1/2 (GPIB) Controller"},
	{0x07, 0x05, "Smart Card"},
	{0x07, 0x80, "Other"},
	{0x08, 0x00, "PIC"},
	{0x08, 0x01, "DMA Controller"},
	{0x08, 0x02, "Timer"},
	{0x08, 0x03, "RTC Controller"},
	{0x08, 0x04, "PCI Hot-Plug Controller"},
	{0x08, 0x05, "SD Host controller"},
	{0x08, 0x06, "IOMMU"},
	{0x08, 0x80, "Other"},
	{0x09, 0x00, "Keyboard Controller"},
	{0x09, 0x01, "Digitizer Pen"},
	{0x09, 0x02, "Mouse Controller"},
	{0x09, 0x03, "Scanner Controller"},
	{0x09, 0x04, "Gameport Controller"},
	{0x09, 0x80, "Other"},
	{0x0A, 0x00, "Generic"},
	{0x0A, 0x80, "Other"},
	{0x0B, 0x00, "386"},
	{0x0B, 0x01, "486"},
	{0x0B, 0x02, "Pentium"},
	{0x0B, 0x03, "Pentium Pro"},
	{0x0B, 0x10, "Alpha"},
	{0x0B, 0x20, "PowerPC"},
	{0x0B, 0x30, "MIPS"},
	{0x0B, 0x40, "Co-Processor"},
	{0x0B, 0x80, "Other"},
	{0x0C, 0x00, "FireWire (IEEE 1394) Controller"},
	{0x0C, 0x01, "ACCESS Bus"},
	{0x0C, 0x02, "SSA"},
	{0x0C, 0x03, "USB Controller"},
	{0x0C, 0x04, "Fibre Channel"},
	{0x0C, 0x05, "SMBus"},
	{0x0C, 0x06, "InfiniBand"},
	{0x0C, 0x07, "IPMI Interface"},
	{0x0C, 0x08, "SERCOS Interface (IEC 61491)"},
	{0x0C, 0x09, "CANbus"},
	{0x0C, 0x80, "Other"},
	{0x0D, 0x00, "iRDA Compatible Controller"},
	{0x0D, 0x01, "Consumer IR Controller"},
	{0x0D, 0x10, "RF Controller"},
	{0x0D, 0x11, "Bluetooth Controller"},
	{0x0D, 0x12, "Broadband Controller"},
	{0x0D, 0x20, "Ethernet Controller (802.1a)"},
	{0x0D, 0x21, "Ethernet Controller (802.1b)"},
	{0x0D, 0x80, "Other"},
	{0x0E, 0x00, "I20"},
	{0x0F, 0x01, "Satellite TV Controller"},
	{0x0F, 0x02, "Satellite Audio Controller"},
	{0x0F, 0x03, "Satellite Voice Controller"},
	{0x0F, 0x04, "Satellite Data Controller"},
	{0x10, 0x00, "Network and Computing Encrpytion/Decryption"},
	{0x10, 0x10, "Entertainment Encryption/Decryption"},
	{0x10, 0x80, "Other Encryption/Decryption"},
	{0x11, 0x00, "DPIO Modules"},
	{0x11, 0x01, "Performance Counters"},
	{0x11, 0x10, "Communication Synchronizer"},
	{0x11, 0x20, "Signal Processing Management"},
	{0x11, 0x80, "Other"},
	{0x00, 0x00, NULL},
};

// Mapping from class -> name.
struct _c_map {
	uint8_t const class;
	char const * const name;
};

// Maps Class to description/name.
static struct _c_map const CLASS_MAP[] = {
	{0x00, "Unclassified"},
	{0x01, "Mass Storage Controller"},
	{0x02, "Network Controller"},
	{0x03, "Display Controller"},
	{0x04, "Multimedia Controller"},
	{0x05, "Memory Controller"},
	{0x06, "Bridge Device"},
	{0x07, "Simple Communication Controller"},
	{0x08, "Base System Peripheral"},
	{0x09, "Input Device Controller"},
	{0x0A, "Docking Station"},
	{0x0B, "Processor"},
	{0x0C, "Serial Bus Controller"},
	{0x0D, "Wireless Controller"},
	{0x0E, "Intelligent Controller"},
	{0x0F, "Satellite Communication Controller"},
	{0x10, "Encryption Controller"},
	{0x11, "Signal Processing Controller"},
	{0x12, "Processing Accelerator"},
	{0x13, "Non-Essential Instrumentation"},
	{0x40, "Co-Processor"},
	{0xFF, "Unassigned Class (Vendor specific)"},
	{0x00, NULL},
};

// Maps a PCI class to its name.
// @param class: The class to map.
// @return: If the class is known, returns a char const * const containing the
// name, else returns NULL.
static inline char const *_class_to_str(uint8_t const class) {
	struct _c_map const * ite = &CLASS_MAP[0];
	for (; ite->name && ite->class != class; ++ite);
	return ite->name;
}

// Maps a PCI (class, subclass) pair to its name.
// @param class: The class to map.
// @param subclass: The subclass to map.
// @return: If the pair is known, returns a char const * const containing the
// name, else returns NULL.
static inline char const *_class_subclass_to_str(uint8_t const class,
                                                 uint8_t const subclass) {
	struct _cs_map const * ite = &CLASS_SUBCLASS_MAP[0];
	for (; ite->name &&
           (ite->class != class || ite->subclass != subclass); ++ite);
	return ite->name;
}

// Maps a PCI (class, subclass, progif) tuple to its name.
// @param class: The class to map.
// @param subclass: The subclass to map.
// @param progif: The progif to map.
// @return: If the tuple is known, returns a char const * const containing the
// name, else returns NULL.
static inline char const *_class_subclass_progif_to_str(uint8_t const class,
                                                        uint8_t const subclass,
                                                        uint8_t const progif) {
	struct _csp_map const * ite = &CLASS_SUBCLASS_PROGIF_MAP[0];
	for (; ite->name &&
           (ite->class != class ||
           ite->subclass != subclass ||
           ite->progif != progif); ++ite);
	return ite->name;
}

// Pretty prints the name of a PCI device given its class, subclass and progif.
// @param class: The class.
// @param subclass: The subclass.
// @param progif: The progif.
static inline void pci_pretty_print_dev(uint8_t const class,
                                        uint8_t const subclass,
                                        uint8_t const progif) {
    char const * const class_str = _class_to_str(class);
    char const * const subclass_str = _class_subclass_to_str(class, subclass);
    char const * const progif_str = _class_subclass_progif_to_str(class,
                                                                  subclass,
                                                                  progif);
    if (class_str) {
        LOG("%s", class_str);
        if (subclass_str) {
            LOG(" / %s", subclass_str);
            if (progif_str) {
                LOG(" / %s", progif_str);
            } else {
                LOG(" / Unknown");
            }
        } else {
            LOG(" / Unknown");
        }
    } else {
        LOG("Unknown");
    }
}
