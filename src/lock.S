#include <macro.h>

#void spinlock_lock(struct spinlock * const lock);
ASM_FUNC_DEF(_spinlock_lock):
    mov     0x4(%esp), %edx
    jmp     test
retry_test:
    pause
test:
    # The lock value is the first element of the spinlock struct so we can
    # dereference edx directly.
    cmpb    $0x0, (%edx)
    jnz     retry_test
    # The lock is free, try to acquire it. 
    mov     $0x0, %al
    mov     $0x1, %cl
    lock cmpxchg    %cl, (%edx)
    jnz     retry_test
    # The lock has been acquired.
    ret

#void spinlock_unlock(struct spinlock * const lock);
ASM_FUNC_DEF(_spinlock_unlock):
    mov     0x4(%esp), %edx
    movb    $0x0, (%edx)
    ret
