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

# Finally define the entry point of the kernel in the .text section, this is
# where life starts !
.section .text
.global _start_kernel
.type _start_kernel, @function
_start_kernel:
	# The very first thing to do is to setup the stack pointer. Note that
	# we are in 32-bit mode still so use %esp.
	mov $stack_top, %esp

	# Enter the main of the kernel.
    # EBX contains the address of the multiboot_info_t structure.
    push    %ebx
	call kernel_main

	# In case we return from the kernel just loop infinitely.
	cli
1:
	hlt
	# We can still "return" from the HALT in case of non-maskable interrupt
	# thus let us be safe and jump back to the halt if it happens.
	jmp 1b
