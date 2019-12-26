#include <macro.h>

ASM_FUNC_DEF(load_gs):
    movw     0x4(%esp), %gs
    ret

ASM_FUNC_DEF(read_byte_gs):
    mov     0x4(%esp), %eax
    mov     %gs:(%eax), %eax
    ret
