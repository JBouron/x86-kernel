#include <macro.h>
.intel_syntax   noprefix

.extern PANIC_ASM
# This routine checks that the index is within the bounds of the bitmap. It
# expects EAX to contain the pointer to the bitmap and ECX the index.
# If the index is not within bounds, the routine triggers a kernel panic.
assert_bounds:
    # Compare the size with the index.
    cmp     ecx, [eax]
    jb      in_range
    # Index is outside of range. 
    # TODO: Call kernel panic from ASM.
    int     3
    # Not reachable
in_range:
    ret

//void bitmap_set(struct bitmap * const bitmap, uint32_t const index);
ASM_FUNC_DEF(bitmap_set):
    push    ebp
    mov     ebp, esp

    # Load ptr into eax.
    mov     eax, [ebp + 0x8]
    # Load index into %ecx.
    mov     ecx, [ebp + 0xC]

    # Check that the index is within bounds.
    call    assert_bounds

    # Index is indeed within bounds. Set the bit.
    mov     eax, [eax + 0x8]
    # The implementation uses a 1 to indicate the entry being free. Therefore
    # setting the bit means writing a 0 in the actual word. This is done with
    # BTR.
    btr     [eax], ecx

    # BTR will read the bit into the CF flag before re-setting it. Read the CF
    # flag into EDX and check if we changed the value.
    pushf
    pop     edx
    and     edx, 1
    # Test if the bit was already set before this call.
    jz     end
    # The bit was not set before this call. Therefore we reduced the number of
    # free bits by one. Reflect that into the `free` field.
    mov     eax, [ebp + 0x8]
    dec     DWORD PTR [eax + 0x4]
end:
    pop     ebp
    ret

//void bitmap_unset(struct bitmap * const bitmap, uint32_t const index);
ASM_FUNC_DEF(bitmap_unset):
    push    ebp
    mov     ebp, esp
    # Load ptr into eax.
    mov     eax, [ebp + 0x8]
    # Load index into %ecx.
    mov     ecx, [ebp + 0xC]

    # Check that the index is within bounds.
    call    assert_bounds

    # Index is indeed within bounds. Unset the bit.
    mov     eax, [eax + 0x8]
    # In the implementation a bit of 0 indicate that the entry is free. Unseting
    # the bit is thus equivalent to writing a 1 in the actual word. This is done
    # with BTS.
    bts     [eax], ecx

    # As with BTR, BTS writes the value of the bit in the CF flag before setting
    # it to 1. Read this value into EDX and check if we need to update the
    # `free` field.
    pushf
    pop     edx
    and     edx, 1
    # The the value of the bit.
    jnz      end2
    # The bit was set before this call. Since we reset it, we increased the
    # number of free bits.
    mov     eax, [ebp + 0x8]
    inc     DWORD PTR [eax + 0x4]
end2:
    pop     ebp
    ret

// uint32_t bitmap_set_next_bit(struct bitmap * const bitmap,
//                              uint32_t const start);
ASM_FUNC_DEF(bitmap_set_next_bit):
    push    ebp
    mov     ebp, esp

    # Load pointer on bitmap into EAX.
    mov     eax, [ebp + 0x8]

    # ECX = start index.
    mov     ecx, [ebp + 0xC]

    # Make sure the requested start index is not out of bounds.
    cmp     ecx, [eax]
    jb      .L_start_index_ok
    # Index out of bounds, return -1.
    pop     ebp
    mov     eax, -1
    ret

.L_start_index_ok:
    # Check if the bitmap is full using the `free` field.
    cmp     DWORD PTR [eax + 0x4], 0x0
    jnz     .L_non_full
    # The bitmap is full, return -1
    mov     eax, -1
    pop     ebp
    ret

.L_non_full:
    # EAX = pointer to data.
    mov     eax, [eax + 0x8]

    # EDI = index of the current DWORD.
    push    edi
    xor     edi, edi

    # Compute the index of DWORD to start from.
    push    ecx
    # ECX = index of DWORD = (ECX / 32)
    shr     ecx, 5
    mov     edi, ecx
    # EAX = pointer to first word to search from.
    lea     eax, [eax + ecx * 4]
    # Note: The BSF will start from bit 0 of the word. Since the start index is
    # not necessarily a multiple of 32, we need to make sure that BSF does not
    # return an index that would be before the start index.
    pop     ecx
    # ECX = start index in current word.
    and     ecx, 0b11111

    push    ebx
    push    esi
.L_find_bit:
    # Load word pointed by EAX
    mov     ebx, [eax]
    test    ebx, ebx
    jz      .L_skip_zero

    # BSF does not take a start index to find the first bit. We need to trick it
    # by setting all bits having an index < ECX to 0.
    test    ecx, ecx
    jz      .L_skip_trick
    mov     esi, -1
    shl     esi, cl
    and     ebx, esi
    test    ebx, ebx
    jz      .L_skip_zero
.L_skip_trick:
    # The current DWORD is not zero (even after doing the trick for the first).
    # Find the first bit in EBX.
    # EBX = index of the first bit set in EBX.
    bsf     ebx, ebx

    # Transform the index from DWORD to bitmap wide index.
    shl     edi, 5
    add     ebx, edi

    # Un-set the bit.
    push    ebx
    push    [ebp + 0x8]
    call    bitmap_set
    add     esp, 0x8
    jmp     .L_done
    
.L_skip_zero:
    # For now on ECX must be 0.
    inc     edi
    xor     ecx, ecx
    add     eax, 0x4
    jmp     .L_find_bit

.L_done:
    # At this point EBX contains the index of the first set bit. The DWORD has
    # been updated accordingly.
    # Clean up and return.
    mov     eax, ebx
    pop     esi
    pop     ebx
    pop     edi
    pop     ebp
    ret

//bool bitmap_get_bit(struct bitmap const * const bitmap, uint32_t const idx);
ASM_FUNC_DEF(bitmap_get_bit):
    push    ebp
    mov     ebp, esp
    # Load ptr into eax.
    mov     eax, [ebp + 0x8]
    # Load index into %ecx.
    mov     ecx, [ebp + 0xC]

    # Check that the index is within bounds.
    call    assert_bounds

    # Index is ok.
    mov     eax, [eax + 0x8]
    bt      [eax], ecx
    # BT puts the bit value in the CF flag (bit 1) of EFLAGS.

    pushf
    pop     eax
    # Since the implementation stores the inverse of the bitmap, we need the NOT
    # here.
    not     eax
    and     eax, 1
    pop     ebp
    ret

//bool bitmap_is_full(struct bitmap const * const bitmap);
ASM_FUNC_DEF(bitmap_is_full):
    mov     eax, [esp + 0x4]
    cmp     DWORD PTR [eax + 0x4], 0x0
    je      full
    mov     eax, 0x0
    ret
full:
    mov     eax, 0x1
    ret

