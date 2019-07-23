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

.section .text
.global __early_boot__enable_paging
.type __early_boot__enable_paging, @function
__early_boot__enable_paging:
    # Enable paging with the page directory address passed as argument but stay
    # in the lower memory.
    mov     4(%esp), %eax
    # %eax now contains the address (physical) of the page directory to be used.
    # We have now everything we need to setup paging.
    # Store page dir address.
    mov     %eax, %cr3
    # Enable paging with protection bits.
    mov     %cr0, %eax
    or      $0x80000001, %eax
    mov     %eax, %cr0
    ret
    


# Finally define the entry point of the kernel in the .text section, this is
# where life starts !
.section .text
.global _start_kernel
.type _start_kernel, @function
_start_kernel:
    # We setup an early boot stack pointer in physical addressing mode before
    # enabling paging.
	mov $(stack_top - KERNEL_START), %esp

    # Note: %ebx contains the physical address of the multiboot header. We will
    # need it later in the kernel_main thus save it into the stack
    push    %ebx

    call __early_boot__setup_paging

    # Get back the saved value of the address of the multiboot header.
    pop     %ebx

    # Paging has been enabled, but we were still using the identity mapping
    # until now. It is time to get into the higher-half kernel with an absolute
    # jump.
    lea 0f, %eax
    jmp %eax
0:

    # We can reset the stack pointer to use a virtual address this time.
	mov $(stack_top), %esp
    # Translate the address of the multiboot header to virtual. This is a simple
    # offset.
    add     $0xC0000000, %ebx
    # Enter the main of the kernel with the address of the multi boot header as
    # argument.
    push    %ebx
    call kernel_main

    # In case we return from the kernel just loop infinitely.
    cli
1:
    hlt
    # We can still "return" from the HALT in case of non-maskable interrupt
    # thus let us be safe and jump back to the halt if it happens.
    jmp 1b
