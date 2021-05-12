#include <macro.h>
.intel_syntax   noprefix

//void spinlock_lock(spinlock_t * const lock, bool const irq_enabled);
ASM_FUNC_DEF(_spinlock_lock):
    push    ebp
    mov     ebp, esp

    push    ebx
    // EDX = lock.
    mov     edx, [ebp + 0x8]
    // EBX = irq_enabled.
    mov     ebx, [ebp + 0xC]

    // Quick and dirty implementation of test and cmpxchg. If requested
    // interrupts will be enabled between two tries while executing the pause
    // instruction.
    jmp     test

backoff:
    // Enable interrupts (if requested) and PAUSE until next try.
    test    ebx, ebx
    jz      no_sti
    sti
no_sti:
    pause

test:
    cli
    // TEST:
    // The lock value is the first element of the spinlock struct so we can
    // dereference edx directly.
    cmp     BYTE PTR [edx], 0
    jnz     backoff

    // CMPXCHG:
    // The lock is free, try to acquire it. 
    xor     al, al
    mov     cl, 1
    lock cmpxchg    [edx], cl
    jnz     backoff
    // The lock has been acquired.
    pop     ebx
    leave
    ret

//void spinlock_unlock(spinlock_t * const lock);
ASM_FUNC_DEF(_spinlock_unlock):
    push    ebp
    mov     ebp, esp
    mov     edx, [ebp + 0x8]
    mov     BYTE PTR [edx], 0
    leave
    ret
