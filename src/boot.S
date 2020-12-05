#include <macro.h>

# Intel syntax >> AT&T syntax. Fight me.
.intel_syntax   noprefix

.set ALIGN,    1 << 0             # Align loaded modules on page boundaries.
.set MEMINFO,  1 << 1             # Provide memory map (bit 6 is deprecated).
.set FLAGS,    ALIGN | MEMINFO
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
    mov     esp, OFFSET FLAT : stack_top
    sub     esp, OFFSET FLAT : KERNEL_PHY_OFFSET

    # We can already enable the cache.
    call    cpu_enable_cache

    # Initialize multiboot.
    # EBX contains the physical address of the multiboot header.
    push    ebx
    call    init_multiboot

    # Even though the multiboot compliant bootloader has created flat segments
    # for us, it is recommended that the OS sets up its own GDT. Do that now
    # before entering the kernel.
    push    OFFSET FLAT : 0f
    call    init_segmentation

    # NOTE: init_segmentation does not return and will far jump into 0f in
    # higher half.
    int3

0:
    # Reset the stack after doing the far jump to higher half linear address.
    # This prevent any garbage.
    mov     esp, OFFSET FLAT : stack_top
    mov     ebp, esp

    # Initialize serial output as soon as we can, that is once we can access
    # higher half logical addresses thanks to the GDT.

    # Initialize output depending on the config.
#ifdef VGA
    call    vga_init
    lea     eax, [VGA_STREAM]
#else
#ifdef SERIAL
    call    serial_init
    lea     eax, [SERIAL_STREAM]
#else
#error No output medium specified at compilation time.
#endif
#endif

    # Init TTY.
    push    eax
    push    0x0
    call    tty_init
    add     esp, 0x8

    # Segments have been setup, including the percpu segment for the BSP. We can
    # now setup the BSP's percpu variables.
    call    init_bsp_boot_percpu

    call    interrupt_init

    # From now on percpu variables of the BSP can be used.

    # Before setting up paging we need to setup the frame allocator so that we
    # can allocate physical frames to hold the kernel page directory and page
    # tables.
    call    init_frame_alloc

    # We can now initialize paging.
    call    init_paging

    # Call the kernel's main function.
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
    # offset 0 in the frame.

    # Disable interrupts before doing anything. Ideally we should also disable
    # NMIs. TODO.
    cli

    # Set other segments to 0, except for ES which will be used to access the
    # data frame.
    xor     ax, ax
    mov     ds, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    # Set ES to point on the data frame. Note: The segment to use for ES is
    # stored at the last word of the physical frame containing the code
    # currently running.
    mov     ax, cs
    mov     es, ax
    mov     ax, es:[0xFFE]
    mov     es, ax

    # Set the GDTR to the temporary GDT provided in the data frame. The
    # descriptor is given at offset 0 in the data segment/frame.
    lgdt    es:[0]


    # Prepare the far pointer to jump into protected mode.
    # The far pointer between real-mode and protected mode is a 32-bit structure
    # with the following layout:
    #   <16-bit segment sel>:<16-bit offset>
    # In our case the segment selector is 0x10 since the code segment in the
    # temporary GDT is at index 2.
    # The offset we want to jump to is target_protected_mode, however, we want
    # to jump to "the" target_protected_mode within the current frame, that is:
    #   CS << 4 + (&target_protected_mode - &ap_entry_point).
    # we _could_ jump to where target_protected_mode has been mapped by the BIOS
    # (that is 0x100000 + (&target_protected_mode - &ap_entry_point) but this
    # would require to ID map this address in order to enable paging. Here, the
    # current frame is already ID mapped.
    #
    # The far jump is done in two steps:
    #   1. Compute the value of the jump. We already have the segment selector
    #      (0x10), we only need to compute the correct offset. We will save this
    #      pointer to ap_entry_point_far_ptr of the current frame.
    #   2. Far jump indirect [ap_entry_point_far_ptr].

    # EBX = Physical address of the start of this routine, that is, where it has
    # been copied by the BSP cpu.
    mov     bx, cs
    movsx   ebx, bx
    shl     ebx, 4

    # EAX = Linear address of target_protected_mode. Note: We cannot use the LEA
    # instruction here as AS would complain that the reolocation would fail
    # (since this code is 16-bit and addresses would be 32-bit). Instead use the
    # OFFSET FLAT: prefix which is equivalent to a LEA but works when mixing
    # 16-bit and 32-bit code.
    mov     eax, OFFSET FLAT : target_protected_mode
    sub     eax, OFFSET FLAT : ap_entry_point
    add     eax, ebx

    # The far pointer will be saved in ap_entry_point_far_ptr.
    # ESI = Address where the far pointer is saved.
    mov     esi, OFFSET FLAT : ap_entry_point_far_ptr
    sub     esi, OFFSET FLAT : ap_entry_point
    add     esi, ebx

    # Save the far pointer into the data segment. All the cpus will write to
    # this location but this is fine since they will write the same value.
    mov     [si], ax
    mov     WORD PTR [si + 2], 0x10

    # Enable PM bit and jump.
    mov     edx, cr0
    or      edx, 1
    mov     cr0, edx
    jmp     FAR PTR [si]

ap_entry_point_far_ptr:
    # This memory location will contain the far pointer to protected mode. This
    # is needed because when bootting, APs will not have a stack until paging is
    # enabled.
.long  0x0

.code32
target_protected_mode:
    # EDI = linear address of the data frame.
    mov     di, es
    movsx   edi, di
    shl     edi, 4

    # We are now in protected mode. The segment registers (except CS) still
    # contain real-mode segment descriptors which are now invalid. Setup up the
    # segment registers DS, ES, FS and GS to use the data segment of the
    # temporary GDT.  Even if we don't use all of them, it does not hurt to set
    # them to sane values.
    mov     ax, 0x8
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    # Setup and enable paging.
    # Set CR3 to contain the value of the page directory of the kernel.
    # EAX = Physical/linear address of page directory.
    mov     eax, [edi + 0x1E]
    # Make sure the 12 LSB of EAX are 0.
    and     eax, ~0xFFF
    mov     cr3, eax

    # Enable PG and WP bits.
    mov     eax, 0x80010000
    mov     ecx, cr0
    or      eax, ecx
    mov     cr0, eax
    
    # Jump to higher half.
    lea     eax, [target_paging] 
    jmp     eax

target_paging:
    # Paging is enabled and we are now in higher half, we can finally use the
    # stack that was allocated for us. The stack is in the kernel_stacks array
    # of the data frame indexed by our APIC ID.

    # EBX = APIC ID
    mov     eax, 1
    # Getting the APIC ID is perfomed by executing cpuid with EAX = 1. The
    # result is encoded in the bits 31:24 of EBX.
    cpuid
    shr     ebx, 24

    # Set stack pointer.
    lea     eax, [edi + 0x26]
    mov     esp, [eax + ebx * 4]

    # We finally have a stack.
    # Setup a fake stack frame for the very bottom of the stack.
    push    0xDEADC0DE
    mov     ebp, esp

    # Finish up the AP initialization.
    call    ap_initialize_state

    # Jump to the target routine. This routine is not suppposed to return.
    mov     eax, [edi + 0x22]
    call    eax

limbo:
    # That should never happen. But if somehow an AP returns and ends up here
    # then lock it up. Goodbye Mr. Cobb.
    cli
    hlt
    jmp limbo

.global ap_entry_point_end
# The ap_entry_point_end label indicates the end of the AP boot code.
ap_entry_point_end:
