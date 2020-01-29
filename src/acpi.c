#include <types.h>
#include <debug.h>
#include <string.h>
#include <memory.h>
#include <kernel_map.h>
#include <kmalloc.h>
#include <paging.h>

// This files contains function related to ACPI (Advanced Configuration and
// Power Interface) parsing.
// At a high level, ACPI is no more than a set of table.
// At the very top is the Root System Descprition Pointer (RSDP). This
// descriptor gives the address of the Root System Description Table (RSDT).
// The RSDT contains a header followed by N pointers pointing to System
// Description Tables.
// Each System Description Table descibes a particular piece of the system:
// APIC, power managment, ... They contain a header common to all tables which
// identifies their type, length, ... The data following tha header depends on
// the type of the table.
//
// All tables in the ACPI interface possesses a signature which identifies their
// nature.
//
// To find the RSDP one need to scan the EBDA to look for the signature of the
// RSDP: "RSD PTR ".
//
// All checksum fields on the following structure is a uint8_t such that adding
// the bytes of the header (or table in certain cases) adds up to 0 mod 256.

// Forward decl of the RSDT to be able to reference to it in the RSDP struct.
struct rsdt_t;

// This is the layout of the RSDP.
struct rsdp_desc_t {
    // The signature of the RSDP, this _must_ be equal to "RSD PTR ".
    char const signature[8];
    // The checksum of the RSDP. Computed by summing the bytes of this struct.
    uint8_t const checksum;
    // The OEM ID.
    char const oemid[6];
    // The version of the ACPI. 0 means v1, 1 means v2. Only v1 is supported for
    // now.
    uint8_t const revision;
    // The physical address of the RSDT.
    struct rsdt_t const * const rsdt_addr;
} __attribute__ ((packed));

// All System Description Table start with a header as follows:
struct sdt_header_t {
    // The signature of the table, that is its nature. Can be "APIC", ...
    char const signature[4];
    // The length of the table INCLUDING this header.
    uint32_t const length;
    // The version of the table.
    uint8_t const revision;
    // Checksum for this table. Note that the checksum of an SDT is computed
    // using the _entire_ table, not just the header.
    uint8_t const checksum;
    // OEM reserved stuff. Not used for now.
    char const oemid[6];
    char const oem_table_id[8];
    uint32_t const oem_revision;
    uint32_t const creator_id;
    uint32_t const creator_revision;
} __attribute__((packed));

// The layout of the Root System Description Table (RSDT).
struct rsdt_t {
    // As all tables in the ACPI the RSDT starts with an SDT header.
    struct sdt_header_t const header;
    // The pointers to the tables referenced by the RSDT. Note that this field
    // has a variable length.
    struct sdt_header_t const * const tables[0];
} __attribute__((packed));

// APIC SDT:
// The APIC SDT (also called MADT for Multiple APIC Description Table) contains
// the SDT header followed by 0 or more variable sized entries. Each entry start
// with a header (madt_entry_header_t) containing one byte indicating the type
// of the entry followed by one byte equal to the size of the entry.

// The MADT layout describing APIC related information of the system. The MADT
// is just a fancy name for the SDT of the APIC subsytem.
struct madt_t {
    // The common header of the SDT.
    struct sdt_header_t const header;
    // The local APIC physical address.
    void * const local_apic_addr;
    // Flags. If this field is set to 1 it means that dual 8259 legacy PICs
    // are installed. Otherwise this field should always be 0.
    uint32_t const flags;
} __attribute__((packed));

enum madt_entry_type_t {
    PROC_LOCAL_APIC = 0,
    IO_APIC = 1,
    INTERRUPT_SOURCE_OVERRIDE = 2,
    NON_MASKABLE_INTERRUPTS = 4,
    LOCAL_APIC_ADDR_OVERRIDE = 5,
} __attribute__((packed));
// This assert makes sure that the size of an enum madt_entry_type_t is the same
// as the size of an uint8_t. That way we can use the enum instead of an uint8_t
// in the madt_entry_header_t struct below.
STATIC_ASSERT(sizeof(enum madt_entry_type_t) == sizeof(uint8_t), "");

// An entry of the MADT table.
struct madt_entry_header_t {
    // Type of the entry.
    enum madt_entry_type_t const type;
    // The length in bytes of the entry INCLUDING this header.
    uint8_t const length;
} __attribute__((packed));

// This structure describes the layout of an MADT entry when the type field of
// the MADT entry header is PROC_LOCAL_APIC (0).
// This entry describes information about the local APIC of a core in the
// system.
struct madt_local_apic_entry_t {
    // The header of the MADT entry.
    struct madt_entry_header_t const header;
    // The ID of the processor in ACPI.
    uint8_t const acpi_processor_id;
    // The ID of the processor in the APIC.
    uint8_t const apic_id;
    // Flags. Bit 0 indicates if the processor is online (0 = offline, 1 =
    // online). If bit 0 is not set then bit 1 is valid and indicate if this
    // processor can be enabled.
    uint32_t const flags;
} __attribute__((packed));

// This structure describes the layout of an MADT entry when the type of  the
// entry is IO_APIC.
struct madt_ioapic_entry_t {
    // The header of the MADT entry.
    struct madt_entry_header_t const header;
    // The ID of the IO APIC described by this entry.
    uint8_t const ioapic_id;
    // Reserved.
    uint8_t const reserved;
    // The physical address of this IO APIC.
    uint32_t const ioapic_addr;
    // The interrupt base of this IO APIC.
    uint32_t const global_sys_int_base;
} __attribute__((packed));

// Compute and verify the checksum of a set of bytes.
// @param ptr: The pointer to the first byte of the data to verify the checksum
// for.
// @param len: The length of the data.
// @return: true if the sum of the bytes between ptr and ptr + len sum to 0 mod
// 256. False otherwise.
static bool verify_checksum_raw(uint8_t const * const ptr, size_t const len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += ptr[i];
    }
    return !sum;
}

// Compute and verify the checksum of an RSDP.
// @param rsdp: The RSDP to verify the checksum for.
// @return: true if the checksum of the RSDP is valid, false otherwise.
static bool verify_rsdp_desc_checksum(struct rsdp_desc_t const * const rsdp) {
    uint8_t const * const bytes = (uint8_t const*) rsdp;
    // The checksm of an RSDP is computed by summing all the byte of the
    // rsdp_desc_t structure.
    return verify_checksum_raw(bytes, sizeof(*rsdp));
}

// Compute and verify the checksum of an SDT.
// @param rsdp: The SDT to verify the checksum for.
// @return: true if the checksum of the SDT is valid, false otherwise.
static bool verify_sdt_checksum(struct sdt_header_t const * const sdt) {
    uint8_t const * const bytes = (uint8_t const*) sdt;
    // Unlike with an RSDP, computing the checksum of an SDT uses all the bytes
    // of the _table_ that is the header AND the data following it.
    return verify_checksum_raw(bytes, sdt->length);
}

// Find the RSDP in low memory.
// @return: A virtual address pointing to the RSDP.
struct rsdp_desc_t const * find_rsdp_desc(void) {
    // Since the EBDA has been mapped to higher half there is no need to map any
    // additional page here.
    void const * const ebda_start = to_virt((void*)0x000A0000);
    void const * const ebda_end = to_virt((void*)0x00100000);
    for (void const * ptr = ebda_start; ptr < ebda_end; ptr += 16) {
        char const * const str = ptr;
        if (strneq(str, "RSD PTR ", 8)) {
            return ptr;
        }
    }
    return NULL;
}

// Get a NULL terminated string containing the signature of an SDT.
// @param sdt: The SDT to get the signature from.
// @return: A dynamically allocated char *, NULL terminated, containing the
// signature of SDT. Note that this char * must be free by the caller of this
// function.
static char * get_sdt_signature(struct sdt_header_t const * const sdt) {
    char * const str = kmalloc(sizeof(sdt->signature) + 1);
    // The allocation above will already memzero the string.
    strncpy(sdt->signature, str, sizeof(sdt->signature));
    return str;
}

// Compute the number of tables references by an RSDT, that is the number of
// physical pointers following the header of the RSDT.
// @param rsdt: The RSDT.
// @return: The number of pointer following RSDT's header.
static uint32_t number_of_tables_for_rsdt(struct rsdt_t const * const rsdt) {
    // After the SDT header, the RSDT contains N pointers to other SDTs. Since
    // we know the size of an SDT header and the total size of the RSDT, the
    // number of pointer can be computed as follows:
    return (rsdt->header.length - sizeof(rsdt->header)) / 4;
}

static void map_table(struct sdt_header_t const * const table) {
    // The goal here is to id map the table, however to this end we need to know
    // its length first. Therefore with first map the header bytes, read the
    // length, and then map the rest.
    paging_map(table, table, sizeof(*table), 0);
    paging_map(table, table, table->length, 0);
}

static void unmap_table(struct sdt_header_t const * const table) {
    paging_unmap(table, table->length);
}

// A callback to be called when parsing the ACPI information.
typedef void (*parser_callback)(struct sdt_header_t const * const);

// Parse the ACPI info and invoke a callback for every SDT found.
// @param callback: A function pointer to be called for every SDT. This function
// is expected to take a callback as argument.
static void parse_acpi_info(parser_callback const callback) {
    // Find the RSDP in memory.
    struct rsdp_desc_t const * const rsdp = find_rsdp_desc();
    if (!rsdp) {
        PANIC("Could not find RSDP");
    }

    // Before proceeding verify the checksum and the version of the RSDP.
    if (!verify_rsdp_desc_checksum(rsdp)) {
        // The checksum is not valid, something went clearly wrong here.
        PANIC("RSDP checksum is invalid.");
    }
    if (rsdp->revision) {
        // This kernel/parser only support ACPI v1.
        PANIC("RSDP is ACPI v2.");
    }

    LOG("RSDP found at %p\n", rsdp);

    // Get a pointer to the Root System Description Table from the RSDP.
    struct rsdt_t const * const rsdt = rsdp->rsdt_addr;

    // The pointer above is a physical pointer and therefore needs to be mapped
    // before parsing.
    map_table(&rsdt->header);

    // Compute the number of tables referenced by the SDT.
    uint32_t const n_tables = number_of_tables_for_rsdt(rsdt);
    LOG("Root SDT contains %u pointers\n", n_tables);

    // Iterate over the array of pointers to SDTs. Call the callback for each
    // SDT.
    for (uint32_t i = 0; i < n_tables; ++i) {
        struct sdt_header_t const * const sdt = rsdt->tables[i];
        // ID map the SDT to virtual memory.
        map_table(sdt);

        if (verify_sdt_checksum(sdt)) {
            // We only call the callback if the checksum matches.
            callback(sdt);
        } else {
            LOG("SDT at %p has an invalid checksum\n", sdt);
        }

        // Remove the mapping for the SDT.
        unmap_table(sdt);

        // FIXME: Since some SDT can be on the same page as the RSDT, the
        // unmap_table(sdt) might unmap the RSDT, leading to pagefault. Make
        // sure this does not happen by re-mapping the RSDT.
        map_table(&rsdt->header);
    }
    unmap_table(&rsdt->header);
}

// Get the string representation of an MADT entry type.
// @param type: The value of the type to get the string representation from.
// @return: A statically allocated string containing the string representation
// of `type`.
static char const * get_madt_type_str(enum madt_entry_type_t const type) {
    switch (type) {
        case PROC_LOCAL_APIC:           return "PROC_LOCAL_APIC";
        case IO_APIC:                   return "IO_APIC";
        case INTERRUPT_SOURCE_OVERRIDE: return "INTERRUPT_SOURCE_OVERRIDE";
        case NON_MASKABLE_INTERRUPTS:   return "NON_MASKABLE_INTERRUPTS";
        case LOCAL_APIC_ADDR_OVERRIDE:  return "LOCAL_APIC_ADDR_OVERRIDE";
        default:                        return "UNKNOWN";
    }
}

// The physical address of the I/O APIC. This global is set during acpi_init().
static void * IO_APIC_ADDR = NULL;

static void process_madt_local_apic_entry(
    struct madt_local_apic_entry_t const * const entry) {
    LOG("   acpi proc id = %u\n", entry->acpi_processor_id);
    LOG("   apic id      = %u\n", entry->apic_id);
    LOG("   flags        = %u\n", entry->flags);
}

static void process_madt_ioapic_entry(
    struct madt_ioapic_entry_t const * const entry) {
    LOG("   ioapic_id           = %u\n", entry->ioapic_id);
    LOG("   ioapic_addr         = %x\n", entry->ioapic_addr);
    LOG("   global_sys_int_base = %x\n", entry->global_sys_int_base);

    // We support only a single IO APIC at the moment.
    ASSERT(!IO_APIC_ADDR);
    IO_APIC_ADDR = (void*)entry->ioapic_addr;
}

// Parses an MADT.
// @param madt: The MADT to parse.
static void process_madt(struct madt_t const * const madt) {
    LOG("MADT is at %p\n", madt);

    // Compute the start and end of the MADT data.
    void const * const end_of_madt = (void*)madt + madt->header.length;
    void const * ptr = (void*)madt + sizeof(*madt);
    uint32_t index = 0;

    // Parse the entries of the MADT.
    while (ptr < end_of_madt) {
        struct madt_entry_header_t const * const header = ptr;
        LOG("MADT[%u] is %s:\n", index, get_madt_type_str(header->type));

        switch (header->type) {
            case PROC_LOCAL_APIC: {
                process_madt_local_apic_entry(ptr);
                break;
            }
            case IO_APIC: {
                process_madt_ioapic_entry(ptr);
                break;
            }
            default:
                break;
        }

        index ++;
        // Go to the next entry.
        ptr = ptr + header->length;
    }
}

// React to an ACPI SDT (or MADT) and parses further. Ignores any other table.
// @param sdt: The SDT to check.
static void acpi_table_callback(struct sdt_header_t const * const sdt) {
    char * const signature = get_sdt_signature(sdt);

    LOG("Found SDT at %p, signature = %s\n", sdt, signature);
    if (streq(signature, "APIC")) {
        struct madt_t const * const madt = (void*)sdt;
        process_madt(madt);
    }
    kfree(signature);
}

void acpi_init(void) {
    // For now we are only interested in the MADT as it contains the IO APIC
    // entry.
    parse_acpi_info(acpi_table_callback);
}

void * acpi_get_ioapic_addr(void) {
    return IO_APIC_ADDR;
}