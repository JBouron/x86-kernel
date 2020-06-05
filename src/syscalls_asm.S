#include <macro.h>

//reg_t do_syscall_dispatch(void const * const func,
//                          struct syscall_args const * const args);
ASM_FUNC_DEF(do_syscall_dispatch):
    // EAX = function ptr.
    mov     0x4(%esp), %eax
    // ECX = pointer on the struct syscall_args.
    mov     0x8(%esp), %ecx
    // Push the arguments for the syscall onto the stack.
    pushl   0x18(%ecx)
    pushl   0x14(%ecx)
    pushl   0x10(%ecx)
    pushl   0x0C(%ecx)
    pushl   0x08(%ecx)
    pushl   0x04(%ecx)

    // Do the syscall.
    call    *%eax

    add     $0x18, %esp
    ret
