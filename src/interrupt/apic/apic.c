#include <interrupt/apic/apic.h>
#include <asm/cpuid/cpuid.h>
#include <asm/asm.h>
#include <utils/debug.h>
#include <memory/paging/paging.h>

// This is the base address of the APIC registers. This is subject to change,
// when paging is enabled for instance.
uint8_t * APIC_REG_ADDR = (uint8_t*)0xFEE00000;

uint8_t
is_apic_present(void) {
    // The APIC is present if calling cpuid with eax = 1 returns edx with bit 9
    // set. If the bit is unset then the feature is not supported.
    uint32_t const eax = 1;
    uint32_t edx = 0;
    cpuid(eax, 0, NULL, NULL, NULL, &edx);
    // Make sure to be careful with the cast to uint8_t since it will overflow.
    return (uint8_t)((edx & (1<<9))>>9);
}

// Write the value `val` into the bit specifying wether or not the APIC is
// enabled on the core. Note that we chose to use the global enable/disable flag
// using the IA32_APIC_BASE_MSR instead of the spurious interrupt vector
// register. (All of this is described in page 3072 of the Intel's manual).
static void
__write_apic_enable_bit(uint8_t const val) {
    ASSERT(val <= 1);
    // Enabling the APIC works by writting a 1 on the bit 11 of the MSR 0x1B.
    uint32_t const IA32_APIC_BASE = 0x1B;
    // Read the current value of the MSR.
    uint64_t msr_value = 0;
    read_msr(IA32_APIC_BASE, &msr_value);

    // The bit 11 of the IA32_APIC_BASE is used to enable (1) or disable (0) the
    // APIC.
    if (val) {
        msr_value |= 1<<11;
    } else {
        msr_value &= ~(1<<11);
    }
    write_msr(IA32_APIC_BASE, msr_value);
}

void
apic_enable(void) {
    __write_apic_enable_bit(1);
}

void
apic_disable(void) {
    __write_apic_enable_bit(0);
}

// Read the register at `offset` from the local APIC.
static uint32_t
__read_apic_register(uint32_t const offset) {
    uint32_t const * const reg = (uint32_t *)(APIC_REG_ADDR + offset);
    return *reg;
}

// Write the register at `offset` from the local APIC.
static void
__write_apic_register(uint32_t const offset, uint32_t const val) {
    uint32_t * const reg = (uint32_t *)(APIC_REG_ADDR + offset);
    *reg = val;
}

// Map the APIC registers to the higher half kernel.
static void
__map_apic_regs(void) {
    // TODO: For now we identity map.
    p_addr const start_apic_regs = (p_addr) APIC_REG_ADDR;
    size_t const apic_reg_size = PAGE_SIZE;
    v_addr const dest = start_apic_regs;
    paging_create_map(start_apic_regs, apic_reg_size, dest);
}

void
apic_init(void) {
    // The very first thing to do is to map the APIC registers to our virtual
    // address space. We need to do that even before the very first interrupt
    // since dealing with an interrupt requires writting to the EOI register.
    __map_apic_regs();
}

void
apic_start_periodic_timer(uint8_t const _vector) {
    // Setup a periodic timer.
    // First setup the initial count.
    uint32_t const INIT_TIMER_REG = 0x380;
    uint32_t const initial_timer_count = 0xFFFFFFF;
    __write_apic_register(INIT_TIMER_REG, initial_timer_count);

    // Setup the timer register in the LVT.
    uint32_t const TIMER_REG = 0x320;
    uint32_t const curr_timer_reg = __read_apic_register(TIMER_REG);
    uint32_t const timer_mode = 1 << 17;    // Periodic timer.
    uint32_t const vector = _vector;             // Interrupt # for timer.
    uint32_t const mask = 0 << 16;          // Unmasked.

    // It is recommended not to touch the reserved bits.
    uint32_t const reserved = curr_timer_reg & (
        (0xFFF80000) |  // Bits 19 to 31.
        (0x0000E000) |  // Bits 13 to 15.
        (0x00000F00));  // Bits 8 to 11.

    uint32_t const new_timer_reg = reserved | timer_mode | vector | mask;
    LOG("New timer reg value : %x\n", new_timer_reg);
    __write_apic_register(TIMER_REG, new_timer_reg);
}

void
apic_eoi(void) {
    uint32_t const EOI_REG = 0xB0;
    // The write to the EOI register indicates the end of interrupt to the local
    // APIC.
    __write_apic_register(EOI_REG, 0);
}
