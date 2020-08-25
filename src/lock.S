#include <macro.h>

//void spinlock_lock(spinlock_t * const lock, bool const irq_enabled);
ASM_FUNC_DEF(_spinlock_lock):
    push    %ebx
    // EDX = lock.
    mov     0x8(%esp), %edx
    // EBX = irq_enabled.
    mov     0xC(%esp), %ebx

    // Quick and dirty implementation of test and cmpxchg. If requested
    // interrupts will be enabled between two tries while executing the pause
    // instruction.
    jmp     test

backoff:
    // Enable interrupts (if requested) and PAUSE until next try.
    test    %ebx, %ebx
    jz      no_sti
    sti
no_sti:
    pause

test:
    cli
    // TEST:
    // The lock value is the first element of the spinlock struct so we can
    // dereference edx directly.
    cmpb    $0x0, (%edx)
    jnz     backoff

    // CMPXCHG:
    // The lock is free, try to acquire it. 
    mov     $0x0, %al
    mov     $0x1, %cl
    lock cmpxchg    %cl, (%edx)
    jnz     backoff
    // The lock has been acquired.
    pop     %ebx
    ret

//void spinlock_unlock(spinlock_t * const lock);
ASM_FUNC_DEF(_spinlock_unlock):
    mov     0x4(%esp), %edx
    movb    $0x0, (%edx)
    ret
