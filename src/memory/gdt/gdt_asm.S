#include <asm/macro.h>

// Definition of __x86_lgdt(struct table_desc const * const).
// This function loads the gdt described by the table descriptor passed as
// argument.
ASM_FUNC_DEF(__x86_lgdt):
    push    %ebp
    mov     %esp, %ebp
    mov     0x8(%ebp), %eax
    lgdt    (%eax)
    pop     %ebp
    ret
