#include <macro.h>
.set ALIGN,    1 << 0             # align loaded modules on page boundaries
.set MEMINFO,  1 << 1             # provide memory map
.set VIDINFO,  1 << 2
.set MEMMAP,   1 << 6  # Detailled map of available memory.
.set FLAGS,    ALIGN | VIDINFO | MEMINFO | MEMMAP # Multiboot 'flag' field.
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
.global stack_bottom
stack_bottom:
# Set the following .skip to set the kernel stack size (common to all
# processors). We use 16KiB which is relatively big for a kernel of that size.
# However this is very useful for testing as we can statically allocate big
# data-structures.
.skip 16384
.global stack_top
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
    sub     $KERNEL_PHY_OFFSET, %esp

    # We can already enable the cache.
    call cpu_enable_cache

    # Note: %ebx contains the physical address of the multiboot header. We will
    # need it later in the kernel_main thus save it into the stack
    push    %ebx

    call    init_multiboot

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

    # Get rid of the pushed ESP, the top of the stack now contains the old value
    # of EBX which is the address of the multiboot header. This is required by
    # the main.
    add     $0x4, %esp

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

.code16
# The align 4KiB is not necessary here, as this code is going to be copied to a
# physical frame under 1MiB anyway. However, it makes debugging much more easier
# as the addresses in the APs EIP and the higher mapped ap_entry_point will have
# their 12 LSB identical.
.align 4096
start_ap_addr_start:
ASM_FUNC_DEF_16BITS(ap_entry_point):
    # This is the entry point for Application processosrs (APs) that is non boot
    # processors. This entry point must be below the 1MiB address as the APs
    # will start executing in real-mode.
    # Note: Any helper functions used in the real-mode or protected mode must be
    # present _after_ this routine, in other words this routine must start at
    # address 0 in the frame.

    # Disable interrupts before doing anything. Ideally we should also disable
    # NMIs. TODO.
    cli

    # Set the data segment to be the same as the code segment. This is needed
    # because the last word in the code frame contains the address of the data
    # frame.
    mov     %cs, %ax
    mov     %ax, %ds

    # Read the data segment that this core should use. This segment is stored in
    # the last word of this code frame.
    mov     0xFFE, %ax
    mov     %ax, %ds

    # Get the APIC ID of this processor. This ID will be used to get a stack
    # from the stack_segments field.
    # BX = temporary Application Processor ID.
    mov     $0x1, %eax
    cpuid
    shr     $0x18, %ebx
    # Zero any registers written by the cpuid above.
    xor     %eax, %eax
    xor     %ecx, %ecx
    xor     %edx, %edx

    # Get the stack segment to use for this cpu. This is stored in the data
    # frame.
    # Normally, cpus are using the stack stored in stack_segments[APIC ID - 1].
    # However, depending on the amount of available memory under 1MiB, multiple
    # cpus may share the same stack. The total number of available stacks in the
    # stack_segments array is given by the num_stacks field of the
    # ap_boot_data_frame_t structure.
    # Therefore, cpus are using stack_segments[APIC ID - 1 % num_stacks].
    # Compute the index within stack_segments:

    # CX = data_frame->num_stacks.
    # 0x24 is the offest of num_stacks in the data frame.
    mov     0x24, %cx

    mov     %bx, %ax
    sub     $0x1, %ax
    # DX = APIC ID - 1 % num_stacks = Index in stack_segments.
    div     %cx

    # Read the stack segment from stack_segments[DX] in to SS.
    mov     $0x28, %ebx
    mov     (%ebx, %edx, 2), %ss

    # Read the stack size and setup the stack pointer. The stack size is at
    # offset 0x22 in the data frame.
    movw    0x22, %sp
    # Skip the stack lock.
    sub     $0x1, %sp

    # Acquire the lock on the stack. Unfortunately we cannot use a helper
    # function for this as the stack may be in use by another cpu. Use a simple
    # cmpxchg without test.
    mov     $0x1, %cl
    jmp     first_try
retry_stack_lock:
    pause
first_try:
    mov     $0x0, %al
    # Note: In real-mode SP cannot be used as a pointer in a memory reference.
    # Bite the bullet and use an address size prefix override on this one by
    # using ESP.
    lock cmpxchg %cl, (%esp)
    jnz     retry_stack_lock

    # Stack lock has been acquired. This cpu can proceed with this stack. We can
    # now call helper functions.

    # Set up the GDT.
    # The GDT descriptor is located at offset 0x0 in the data frame.
    mov     $0x0, %eax
    lgdt    (%eax)

    # Save the page directory address for later when we will enable paging.
    # The PD address is at offset 0x1F in the data frame.
    pushl   0x1E

    # Create the far pointer used to jump into protected mode.
    call    create_far_pointer_to_protected_mode

    # Push the far pointer onto the stack and jump to it after enabling
    # protected mode in the control registers CR0.
    push    %eax
    call    enable_and_jump_to_protected_mode
    # This call will never return.

# Below are the helper function for the 16-bits entry point above.

# Create the far pointer to jmp into protected mode. The far pointer being a
# 4-bytes data structure will be returned in EAX.
create_far_pointer_to_protected_mode:
    # Construct the far pointer. The far pointer is constitued of two words:
    #   _ The code segment selector to use in protected mode.
    #   _ The address of the target to jump to.
    # In a 32-bit register this would look like: <segment><addr>.

    # The target for the jump into protected mode is target_protected_mode.
    # One quirk of this jump is the fact that the target address of the far
    # pointer needs to be a linear address, as we are jumping into protected
    # mode.
    # When computing the address of the target, we have two options:
    #   1. Use the physical address of the target, where it has been loaded by
    #   the boot loader eg. 0x100000 + <target>.
    #   2. Use the physical address of the target where it has been placed by
    #   the BSP when copying this code frame eg.: 
    #       <ap_entry_point>+<target - ap_entry_point>
    # To jump into protected mode, both ways are valid. However the second
    # option is more attractive when we want to enable paging because the
    # current code frame is already ID mapped in virtual memory. Using option 1
    # would require yet another ID mapping for the code frame containing the
    # target.
    # Therefore we use the address of the target within the current code frame.

    # Get the offset of the target from the start of the code frame (starting at
    # ap_entry_point).
    mov     $target_protected_mode, %eax
    sub     $ap_entry_point, %eax

    # Since the target is a linear address, compute it by adding the current
    # code segment shifted by 4 to the left.
    mov     %cs, %dx
    shl     $4, %dx
    # EAX = linear address for the target of the far jump.
    add     %edx, %eax

    # In the temporary GDT, the code segment to use is 0x10, that is entry with
    # index 2 and privilege level 0.
    # ECX = protected mode code segment to use.
    mov     $0x10, %ecx

    # Build EAX to contain the far pointer, that is the code segment in the high
    # 16 bits followed by the address of the target in the lower 16 bits.
    shl     $16, %ecx
    or      %ecx, %eax

    # Return the far pointer.
    ret

# Enable protected mode and perform a far jump into protected mode using the
# pointer passed as paramter in the stack.
# @param: The 16bits far pointer (4 bytes) to jump to in protected mode.
# Note: This routine will not return.
enable_and_jump_to_protected_mode:
    # Clean up our stack. We need to remove the return address and the far
    # pointer passed as parameter: so 0x2 (16-bits return addr) + 0x4.
    add     $0x6, %sp

    # Enable Protected mode by setting the PE bit (0) of CR0.
    mov     %cr0, %edx
    or      $1, %edx
    mov     %edx, %cr0

    # We can now execute the long jump into protected mode. This must be done
    # right after the mov to cr0 above. If any instruction is in between the
    # move and the jump the behaviour is undefined, per Intel's manual.
    # The far pointer passed as argument is now located at %esp - 0x4 since we
    # clean up the stack pointer above.
    ljmp    *-0x4(%esp)

# End of helper function for real-mode code.

.code32
target_protected_mode:
    # We are now in protected mode. The segment registers (except CS) still
    # contain real-mode segment descriptor which are now invalid. Setup up the
    # segment registers DS, ES, FS and GS to use the data segment of the
    # temporary GDT.  Even if we don't use all of them, it does not hurt to set
    # them to sane values.
    mov     $0x8, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs

    # The stack is still in real-mode addressing, we need to convert it to
    # linear addressing.
    # Compute the linear address of the current stack pointer given the
    # real-mode stack segment and stack pointer.
    mov     %ss, %eax
    shl     $4, %eax
    add     %eax, %esp
    # Use the 0x8 (Data segment of temp. GDT) for the stack segment.
    mov     $0x8, %ax
    mov     %ax, %ss

    # Segments are now valid and we can call functions again. Note: Calling a
    # function from protected mode requires computing the function's physical
    # address and making an absolute call to it. The absolute call is of outmost
    # importance since this code frame only has been relocated and therefore
    # relative calls fail.
    # The CALL_FROM_PROTECTED_MODE macro does all of this for us.

    # Now that we are in protected mode we can setup and enable paging.

    # The current top of the stack contains the physical address of the kernel
    # pag directory to use. Therefore we can already call setup_paging.
    call    setup_paging
    add     $0x4, %esp

    # Paging has been setup, we are ready to jump into the higher half kernel
    # code.
    call    jump_to_higher_half
    # This call will not return.

# Helper functions for protected mode code:

# Setup the CR3 register with the address of the page directory to use and
# enable the paging bits. After this function paging is enabled.
# Note: The physical frame holding this code needs to be ID mapped to avoid a
# pagefault when enabling paging.
# @param: The physical address to load into CR3.
setup_paging:
    # Setup the CR3 register to point to the page directory to be used.
    # Push the address of the page directory to pass it as parameter to
    # cpu_set_cr3.
    push    0x4(%esp)
    CALL_FROM_PROTECTED_MODE(cpu_set_cr3)

    # Pop the page directory addr from the stack.
    add     $0x4, %esp

    # Enable paging by setting the corresponding bits in CR0.
    CALL_FROM_PROTECTED_MODE(cpu_enable_paging_bits)

    # Note: We are still in the ID mapped code frame of the start up code here.
    ret

# Jump into the higher half kernel. This routine will not return.
# The processor will jump into the virtual page containing the AP booting code.
jump_to_higher_half:
    # Before jumping into the higher half target, we need to clean up the stack
    # from the return address (as this routine will never return).
    add     $0x4, %esp

    # We can now jump into the higher half kernel.
    mov     $target_paging, %eax
    jmp     *%eax
# End of helper functions for protected mode.

# The target of jump_to_higher_half. APs will end up here (in higher half mapped
# virtual address space) after calling jump_to_higher_half.
target_paging:
    # Note: Since we are in higher half we can now call functions without
    # resorting to absolute calls tricks.

    # Allocate a new, bigger stack in higher half kernel for this cpu.
    call    ap_alloc_higher_half_stack
    
    # We do not need the old boot stack anymore. Unlock it to allow another
    # processor to use it in case boot stacks are shared.
    movb    $0x0, (%esp)

    # Use the allocated stack above. This cpu is now free to do whatever it
    # wants.

    # Compute the size of a kernel stack.
    mov     $(stack_top - stack_bottom), %ebx
    # Add the size of the stack, this gives the address of the bottom of the
    # stack and therefore the address we should use as ESP. This is very
    # important as ap_alloc_higher_half_stack returns the address of the _top_
    # of the stack.
    add     %ebx, %eax

    mov     %eax, %esp

    # Jump to the final start up routine. This routine is not supposed to
    # return.
    jmp     ap_finalize_start_up

limbo:
    # That should never happen. But if somehow an AP returns and ends up here
    # then lock it up.
    cli
    hlt
    jmp limbo

.global ap_entry_point_end
# The ap_entry_point_end label indicates the end of the AP boot code.
ap_entry_point_end:
