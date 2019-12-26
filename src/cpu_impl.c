#include <cpu.h>

// Those functions read the raw value of the segment registers.
extern uint16_t __cpu_read_cs(void);
extern uint16_t __cpu_read_ds(void);
extern uint16_t __cpu_read_es(void);
extern uint16_t __cpu_read_fs(void);
extern uint16_t __cpu_read_gs(void);
extern uint16_t __cpu_read_ss(void);

// The following functions return the segment selector stored into the segment
// registers.
union segment_selector_t cpu_read_cs(void) {
    union segment_selector_t const sel = {
        .value = __cpu_read_cs(),
    };
    return sel;
}

union segment_selector_t cpu_read_ds(void) {
    union segment_selector_t const sel = {
        .value = __cpu_read_ds(),
    };
    return sel;
}

union segment_selector_t cpu_read_es(void) {
    union segment_selector_t const sel = {
        .value = __cpu_read_es(),
    };
    return sel;
}

union segment_selector_t cpu_read_fs(void) {
    union segment_selector_t const sel = {
        .value = __cpu_read_fs(),
    };
    return sel;
}

union segment_selector_t cpu_read_gs(void) {
    union segment_selector_t const sel = {
        .value = __cpu_read_gs(),
    };
    return sel;
}

union segment_selector_t cpu_read_ss(void) {
    union segment_selector_t const sel = {
        .value = __cpu_read_ss(),
    };
    return sel;
}
