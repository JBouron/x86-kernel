#include <macro.h>

//void atomic_add(atomic_t * const atomic, int32_t const value);
ASM_FUNC_DEF(atomic_add):
    mov     0x4(%esp), %eax
    mov     0x8(%esp), %ecx
    lock add    %ecx, (%eax)
    ret

//int32_t atomic_fetch_and_add(atomic_t * const atomic, int32_t const value);
ASM_FUNC_DEF(atomic_fetch_and_add):
    mov     0x4(%esp), %ecx
    mov     0x8(%esp), %eax
    lock xaddl   %eax, (%ecx)
    // At this point $EAX contains the old value of the atomic.
    ret

//bool atomic_dec_and_test(atomic_t * const atomic);
ASM_FUNC_DEF(atomic_dec_and_test):
    mov     0x4(%esp), %eax
    lock decl   (%eax) 
    mov     $0x0, %eax
    mov     $0x1, %ecx
    cmovz   %ecx, %eax
    ret
