#include <macro.h>
.intel_syntax   noprefix

//void atomic_add(atomic_t * const atomic, int32_t const value);
ASM_FUNC_DEF(atomic_add):
    mov     eax, [esp + 4]
    mov     ecx, [esp + 8]
    lock add    [eax], ecx
    ret

//int32_t atomic_fetch_and_add(atomic_t * const atomic, int32_t const value);
ASM_FUNC_DEF(atomic_fetch_and_add):
    mov     ecx, [esp + 4]
    mov     eax, [esp + 8]
    lock xadd   DWORD PTR [ecx], eax
    // At this point $EAX contains the old value of the atomic.
    ret

//bool atomic_dec_and_test(atomic_t * const atomic);
ASM_FUNC_DEF(atomic_dec_and_test):
    mov     eax, [esp + 4]
    lock dec    DWORD PTR [eax]
    mov     eax, 0
    mov     ecx, 1
    cmovz   eax, ecx
    ret
