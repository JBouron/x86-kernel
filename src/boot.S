#include <macro.h>
.set ALIGN,    1 << 0             # align loaded modules on page boundaries
.set MEMINFO,  1 << 1             # provide memory map
.set VIDINFO,  1 << 2
.set FLAGS,    ALIGN | VIDINFO | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic num' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

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

.section .text
# The entry point of the kernel. We are live.
ASM_FUNC_DEF(_start_kernel):
    # Multiboot is supposed to disable interrupts for us, but really, it doesn't
    # hurt to be extra careful and disable them ourselves in case it wasn't
    # done.
    cli

    # We setup an early boot stack pointer in physical addressing mode before
    # enabling paging.
    mov     $(stack_top), %esp
    sub     $KERNEL_START, %esp

    # Note: %ebx contains the physical address of the multiboot header. We will
    # need it later in the kernel_main thus save it into the stack
    push    %ebx

    # Even though the multiboot compliant bootloader has created flat segments
    # for us, it is recommended that the OS sets up its own GDT. Do that now
    # before entering the kernel.
    call    init_segmentation

    # Before setting up paging we need to setup the frame allocator so that we
    # can allocate physical frames to hold the kernel page directory and page
    # tables.
    call    init_frame_alloc

    # We can now initialize paging.
    push    %esp
    call    init_paging

    # Enter the main of the kernel with the address of the multi boot header as
    # argument.
    call    kernel_main

    # In case we return from the kernel just loop infinitely.
    cli
1:
    hlt
    # We can still "return" from the HALT in case of non-maskable interrupt
    # thus let us be safe and jump back to the halt if it happens.
    jmp     1b
