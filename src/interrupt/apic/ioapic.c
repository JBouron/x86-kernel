#include <interrupt/apic/ioapic.h>
#include <memory/paging/paging.h>

// See https://pdos.csail.mit.edu/6.828/2018/readings/ia32/ioapic.pdf for
// reference.

// Write `reg` into the IOREGSEL of the given IOAPIC.
static void
__ioapic_write_ioregsel(v_addr const ioapic, uint32_t const reg) {
    // Note: We need to read the current value of the IOREFSEL, update it and
    // write it back, as specified by the reference.
    volatile uint32_t * const ioregsel = (uint32_t*)(ioapic + IOAPIC_IOREGSEL);
    uint32_t const prev_ioregsel = *ioregsel;
    uint32_t const new_ioregsel = (prev_ioregsel & (~(0xFF))) | reg;
    *ioregsel = new_ioregsel;
}

// Read the register `reg` from the ioapic.
static uint32_t
__ioapic_read(v_addr const ioapic, uint32_t const reg) {
    // First set the IOREGSEL to the value of the register we want to read from.
    __ioapic_write_ioregsel(ioapic, reg);
    // Now we can read from IOWIN to get the value of the register `reg`.
    volatile uint32_t const * const iowin = (uint32_t*)(ioapic + IOAPIC_IOWIN);
    return *iowin;
}

// Write the register `reg` of the ioapic.
static void
__ioapic_write(v_addr const ioapic, uint32_t const reg, uint32_t const value) {
    // First set the IOREGSEL to the value of the register we want to write
    // from.
    __ioapic_write_ioregsel(ioapic, reg);
    // Now write to IOWIN, this will write into the register `reg`.
    volatile uint32_t * const iowin = (uint32_t*)(ioapic + IOAPIC_IOWIN);
    *iowin = value;
}

// Write the full 64 bits of the entry `tblidx` of the redirection table.
static void
__write_redtbl(v_addr const ioapic, uint8_t const tblidx, uint64_t const val) {
    uint32_t const hi = val >> 32;
    uint32_t const lo = val & 0xFFFFFFFF;

    // Writing a 64 bit value is equivalent to writing two 32 bits values per
    // the reference.
    __ioapic_write(ioapic, IOAPIC_REG_IOREDTBL(tblidx)+0, lo);
    __ioapic_write(ioapic, IOAPIC_REG_IOREDTBL(tblidx)+1, hi);
}

// Check the existance of an IOAPIC at `ioapic_addr` and dump info into logs.
static void
__check_ioapic(v_addr const ioapic_addr) {
    uint32_t const id = __ioapic_read(ioapic_addr, IOAPIC_REG_IOAPICID);
    uint32_t const vermaxred = __ioapic_read(ioapic_addr, IOAPIC_REG_IOAPICVER);
    uint32_t const version = vermaxred & 0xFF;
    uint32_t const maxredir = (vermaxred & 0x00FF0000) >> 16;
    uint32_t const arb = __ioapic_read(ioapic_addr, IOAPIC_REG_IOAPICARB);  
    LOG("New IOAPIC set up:\n");
    LOG("I/O APIC id = %d\n", id);
    LOG("I/O APIC version = %d\n", version);
    LOG("I/O APIC max redir = %d\n", maxredir);
    LOG("I/O APIC arb = %d\n", arb);

    // Some asserts on the values with expect above.
    ASSERT(version == 17);
    ASSERT(maxredir == 23);
}

// Map the ioapic in virtual address space and return the address in virtual
// address space. For now we use identity mapping.
static v_addr
__map_ioapic(p_addr const ioapic_addr) {
    size_t const size = PAGE_SIZE;
    v_addr const dest = ioapic_addr;
    paging_create_map(ioapic_addr, size, dest);
    return (v_addr)ioapic_addr;
}

// Initialize the default IO APIC.
void
ioapic_init(void) {
    p_addr const default_ioapic = IOAPIC_DEFAULT_PHY_ADDR;
    // Map the ioapic into virtual memory.
    v_addr const ioapic_v = __map_ioapic(default_ioapic);
    // Make some checks that we have indeed an IOAPIC at this location.
    __check_ioapic(ioapic_v);

    // Setup redirection entry for interrupt 4 (serial) redirected to interrupt
    // 0x21. TODO: This should be somewhere else.
    uint64_t const vec = 0x21;
    uint64_t const val = vec;
    __write_redtbl(ioapic_v, 4, val);
}
