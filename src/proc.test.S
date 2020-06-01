#include <macro.h>

ASM_FUNC_DEF(switch_to_proc_test_prog):
.global switch_to_proc_test_prog_start
switch_to_proc_test_prog_start:
loop:
    mov     $0xDEADBEEF, %eax
    jmp     loop
.global switch_to_proc_test_prog_end
switch_to_proc_test_prog_end:
