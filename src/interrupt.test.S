#include <macro.h>
.intel_syntax   noprefix

ASM_FUNC_DEF(basic_interrupt_test_int):
    int     0x0
    ret

.extern basic_interrupt_test_target
ASM_FUNC_DEF(basic_interrupt_test_handler):
    movb     [basic_interrupt_test_target], 0xAA
    iret
