#include <asm/macro.h>
.set ALIGN,    1 << 0             # align loaded modules on page boundaries
.set MEMINFO,  1 << 1             # provide memory map
.set VIDINFO,  1 << 2
.set FLAGS,    ALIGN | VIDINFO | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic num' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot
.set KERNEL_START,   0xC0000000  # Start address of the kernel.

.section .multiboot
# The multiboot section needs to be 32-bit aligned.
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.skip 24 # Skip the entries of the multiboot header corresponding to flag[16].
.long 0 # No preference for width.
.long 0 # No preference for height.
.long 0 # No preference for depth.

# Define the stack here. By default use 16KiB.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16KiB.
stack_top:

ASM_FUNC_DEF(__get_stack_top):
    mov $(stack_top), %eax
    ret

# Finally define the entry point of the kernel in the .text section, this is
# where life starts !
ASM_FUNC_DEF(_start_kernel):
    # We setup an early boot stack pointer in physical addressing mode before
    # enabling paging.
	mov $(stack_top - KERNEL_START), %esp

    # Note: %ebx contains the physical address of the multiboot header. We will
    # need it later in the kernel_main thus save it into the stack
    push    %ebx

    # Enter the main of the kernel with the address of the multi boot header as
    # argument.
    call kernel_main

    # In case we return from the kernel just loop infinitely.
    cli
1:
    hlt
    # We can still "return" from the HALT in case of non-maskable interrupt
    # thus let us be safe and jump back to the halt if it happens.
    jmp 1b
