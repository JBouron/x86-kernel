#include <macro.h>
.intel_syntax   noprefix

# Translate a virtual address to a physical one. Note that this function must be
# called with a valid stack pointer.
# void * to_phys(void const * const ptr);
ASM_FUNC_DEF(to_phys):
    mov     eax, [esp + 0x4]
    sub     eax, OFFSET FLAT : KERNEL_PHY_OFFSET
    ret

# Translate a physical address to a virtual one. Note that this function must be
# called with a valid stack pointer.
# void * to_virt(void const * const ptr);
ASM_FUNC_DEF(to_virt):
    mov     eax, [esp + 0x4]
    add     eax, OFFSET FLAT : KERNEL_PHY_OFFSET
    ret

#void * get_page_addr(void const * const ptr);
ASM_FUNC_DEF(get_page_addr):
    mov     eax, 0xFFF
    not     eax
    and     eax, [esp + 0x4]
    ret
