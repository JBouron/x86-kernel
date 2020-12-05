#include <macro.h>
.intel_syntax   noprefix

//void spinlock_lock(spinlock_t * const lock, bool const irq_enabled);
ASM_FUNC_DEF(_spinlock_lock):
    push    ebx
    // EDX = lock.
    mov     edx, [esp + 0x8]
    // EBX = irq_enabled.
    mov     ebx, [esp + 0xC]

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
    mov     al, 0
    mov     cl, 1
    lock cmpxchg    [edx], cl
    jnz     backoff
    // The lock has been acquired.
    pop     ebx
    ret

//void spinlock_unlock(spinlock_t * const lock);
ASM_FUNC_DEF(_spinlock_unlock):
    mov     edx, [esp + 0x4]
    mov     BYTE PTR [edx], 0
    ret
