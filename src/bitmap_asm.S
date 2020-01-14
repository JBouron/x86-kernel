#include <macro.h>

.extern PANIC_ASM
# This routine checks that the index is within the bounds of the bitmap. It
# expects EAX to contain the pointer to the bitmap and ECX the index.
# If the index is not within bounds, the routine triggers a kernel panic.
assert_bounds:
    # Compare the size with the index.
    cmp     (%eax), %ecx
    jb      in_range
    # Index is outside of range. 
    # TODO: Call kernel panic from ASM.
    int     $3
    # Not reachable
in_range:
    ret

//void bitmap_set(struct bitmap_t * const bitmap, uint32_t const index);
ASM_FUNC_DEF(bitmap_set):
    push    %ebp
    mov     %esp, %ebp    

    # Load ptr into eax.
    mov     0x8(%ebp), %eax
    # Load index into %ecx.
    mov     0xC(%ebp), %ecx

    # Check that the index is within bounds.
    call    assert_bounds

    # Index is indeed within bounds. Set the bit.
    mov     0x8(%eax), %eax
    # The implementation uses a 1 to indicate the entry being free. Therefore
    # setting the bit means writing a 0 in the actual word. This is done with
    # BTR.
    btr     %ecx, (%eax)

    # BTR will read the bit into the CF flag before re-setting it. Read the CF
    # flag into EDX and check if we changed the value.
    pushf
    pop     %edx
    and     $0x1, %edx
    # Test if the bit was already set before this call.
    jz     end
    # The bit was not set before this call. Therefore we reduced the number of
    # free bits by one. Reflect that into the `free` field.
    mov     0x8(%ebp), %eax
    decl    0x4(%eax)
end:
    pop     %ebp
    ret

//void bitmap_unset(struct bitmap_t * const bitmap, uint32_t const index);
ASM_FUNC_DEF(bitmap_unset):
    push    %ebp
    mov     %esp, %ebp    
    # Load ptr into eax.
    mov     0x8(%ebp), %eax
    # Load index into %ecx.
    mov     0xC(%ebp), %ecx

    # Check that the index is within bounds.
    call    assert_bounds

    # Index is indeed within bounds. Unset the bit.
    mov     0x8(%eax), %eax
    # In the implementation a bit of 0 indicate that the entry is free. Unseting
    # the bit is thus equivalent to writing a 1 in the actual word. This is done
    # with BTS.
    bts     %ecx, (%eax)

    # As with BTR, BTS writes the value of the bit in the CF flag before setting
    # it to 1. Read this value into EDX and check if we need to update the
    # `free` field.
    pushf
    pop     %edx
    and     $0x1, %edx
    # The the value of the bit.
    jnz      end2
    # The bit was set before this call. Since we reset it, we increased the
    # number of free bits.
    mov     0x8(%ebp), %eax
    incl    0x4(%eax)
end2:
    pop     %ebp
    ret

//uint32_t bitmap_set_next_bit(struct bitmap_t * const bitmap);
ASM_FUNC_DEF(bitmap_set_next_bit):
    push    %ebp
    mov     %esp, %ebp

    # Load pointer on bitmap into EAX.
    mov     0x8(%ebp), %eax

    # Check if the bitmap is full using the `free` field.
    cmp     $0x0, 0x4(%eax)
    jnz      non_full
    # The bitmap is full, return -1
    mov     $-1, %eax
    pop     %ebp
    ret

non_full:
    # Some free bits are available try to find them. To this end we use the BSF
    # instruction which looks for the least significant bit set in a 32bits
    # word. The idea is thus to find the first word with a free bit (that is
    # that is not 0) and execute BFS on it. EAX will point to such a word.
    mov     0x8(%eax), %eax
    # EDX counts the number of full words encountered before finding one with a
    # free bit.
    mov     $0x0, %edx
loop:
    cmp     $0x0, (%eax)
    jne     found
    # This word is 0, no free bit available. Get to the next one.
    add     $0x4, %eax
    add     $0x1, %edx
    jmp     loop

found:
    # EAX now points to a word with a free bit. Get the index of the next free
    # bit in ECX. Find the first bit in it.
    bsf     (%eax), %ecx

    # ECX is _local_ to the word, not the full bitmap. Hence add EDX * 32 to it.
    shl     $5, %edx
    add     %edx, %ecx

    # Set the bit. That it write 0.
    push    %ecx
    push    0x8(%ebp) 
    call    bitmap_set
    add     $0x4, %esp
    pop     %ecx

    # Return the index.
    mov     %ecx, %eax
    pop     %ebp
    ret
    
//bool bitmap_get_bit(struct bitmap_t const * const bitmap, uint32_t const idx);
ASM_FUNC_DEF(bitmap_get_bit):
    push    %ebp
    mov     %esp, %ebp    
    # Load ptr into eax.
    mov     0x8(%ebp), %eax
    # Load index into %ecx.
    mov     0xC(%ebp), %ecx

    # Check that the index is within bounds.
    call    assert_bounds

    # Index is ok.
    mov     0x8(%eax), %eax
    bt      %ecx, (%eax)
    # BT puts the bit value in the CF flag (bit 1) of EFLAGS.

    pushf
    pop     %eax
    # Since the implementation stores the inverse of the bitmap, we need the NOT
    # here.
    not     %eax
    and     $0x1, %eax
    pop     %ebp
    ret

//bool bitmap_is_full(struct bitmap_t const * const bitmap);
ASM_FUNC_DEF(bitmap_is_full):
    mov     0x4(%esp), %eax
    cmp     $0x0, 0x4(%eax)
    je      full
    mov     $0x0, %eax
    ret
full:
    mov     $0x1, %eax
    ret

