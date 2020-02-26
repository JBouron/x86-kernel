#include <macro.h>

//void *_read_void_ptr_at_offset(void);
ASM_FUNC_DEF(_read_void_ptr_at_offset):
    mov     0x4(%esp), %eax
    mov     %gs:(%eax), %eax
    ret
