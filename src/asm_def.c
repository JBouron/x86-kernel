#include <asm.h>

// The actual implementation of read_msr, splitting the 64bit uint into high and
// low double words.
extern void
__read_msr(uint8_t const msr_num, uint32_t * const hi, uint32_t * const low);

// The actual implementation of write_msr, splitting the 64bit uint into high
// and low double words.
extern void
__write_msr(uint8_t const msr_num, uint32_t const hi, uint32_t const low);

void
read_msr(uint8_t const msr_num, uint64_t const * dest) {
    // Just split the dest into two double word pointers.
    uint32_t * const double_words = (uint32_t*)dest;
    // XXX: We assume LSB here !!
    uint32_t * const low = double_words;
    uint32_t * const high = double_words+1;
    __read_msr(msr_num, high, low);
}

void
write_msr(uint8_t const msr_num, uint64_t const val) {
    // XXX: We assume LSB here !!
    uint32_t const low = 0xFFFFFFFF & val;
    uint32_t const high = val >> 32;
    __write_msr(msr_num, high, low);
}
