#include <macro.h>
.intel_syntax   noprefix

ASM_FUNC_DEF(load_gs):
    mov     gs, [esp + 0x4]
    ret

ASM_FUNC_DEF(read_byte_gs):
    mov     eax, [esp + 0x4]
    mov     eax, gs:[eax]
    ret
