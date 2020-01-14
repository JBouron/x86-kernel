#include <macro.h>

# Translate a virtual address to a physical one. Note that this function must be
# called with a valid stack pointer.
# void * to_phys(void const * const ptr);
ASM_FUNC_DEF(to_phys):
    mov     0x4(%esp), %eax
    sub     $KERNEL_PHY_OFFSET, %eax
    ret

# Translate a physical address to a virtual one. Note that this function must be
# called with a valid stack pointer.
# void * to_virt(void const * const ptr);
ASM_FUNC_DEF(to_virt):
    mov     0x4(%esp), %eax
    add     $KERNEL_PHY_OFFSET, %eax
    ret

