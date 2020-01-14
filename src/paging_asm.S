#include <macro.h>

#void fixup_esp_and_ebp_to_virt(void);
ASM_FUNC_DEF(fixup_esp_and_ebp_to_virt):
    push    %esp
    call    to_virt
    # By moving %eax in %esp, %esp should now point to the same word in the
    # stack as before the push above.
    mov     %eax, %esp
    # Do the same for EBP.
    push    %ebp
    call    to_virt
    mov     %eax, %ebp
    add     $0x4, %esp
    ret

