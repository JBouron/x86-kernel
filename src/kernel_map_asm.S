#include <macro.h>
.intel_syntax   noprefix

# Translate a virtual address to a physical one.
# void * to_phys(void const * const ptr);
ASM_FUNC_DEF(to_phys):
    mov     eax, [esp + 0x4]
    sub     eax, OFFSET FLAT : KERNEL_PHY_OFFSET
    ret

# Translate a physical address to a virtual one.
# void * to_virt(void const * const ptr);
ASM_FUNC_DEF(to_virt):
    mov     eax, [esp + 0x4]
    add     eax, OFFSET FLAT : KERNEL_PHY_OFFSET
    ret

#void * get_page_addr(void const * const ptr);
ASM_FUNC_DEF(get_page_addr):
    mov     eax, [esp + 0x4]
    and     eax, ~0xFFF
    ret
