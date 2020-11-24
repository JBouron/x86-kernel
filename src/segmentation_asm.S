#include <macro.h>

ASM_FUNC_DEF(set_higher_half_segments):
    // Load the segment descriptors.
    mov     0x4(%esp), %eax
    mov     0x8(%esp), %cx
    mov     0xC(%esp), %dx

    // Update the segment registers.
    mov     %cx, %ds
    mov     %cx, %es
    mov     %cx, %fs
    mov     %cx, %ss
    mov     %dx, %gs

    // Fixup the ESP and EBP.
    add     $0xC0000000, %esp
    add     $0xC0000000, %ebp

    // We can now use the stack again. Prepare the far jump to the target in the
    // new code segment.
    mov     0x10(%esp), %ecx
    push    %eax
    push    %ecx

    // Do the far jump
    ljmp    *(%esp)
