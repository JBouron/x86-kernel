#include <macro.h>
.intel_syntax   noprefix

//void *_read_void_ptr_at_offset(void);
ASM_FUNC_DEF(_read_void_ptr_at_offset):
    mov     eax, [esp + 0x4]
    mov     eax, gs:[%eax]
    ret
