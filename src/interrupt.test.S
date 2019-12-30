#include <macro.h>

ASM_FUNC_DEF(basic_interrupt_test_int):
    int     $0x0
    ret

.extern basic_interrupt_test_target
ASM_FUNC_DEF(basic_interrupt_test_handler):
    movb     $0xAA, basic_interrupt_test_target
    iret
