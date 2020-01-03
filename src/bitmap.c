#include <bitmap.h>
#include <math.h>
#include <memory.h>

// Most of the bitmap implementation is done in assembly in bitmap_asm.S. This
// file contains functions that we not deemed necessary to be written in
// assembly.
//
// Implementation note:
//   The reason for implementing the bitmap in assembly is to use the bitmap
// instruction of x86 (BTS, BTR, BSF). The most important instruction is BSF,
// which allows one to find a bit set in a word/bitmap. Unfortunately there is
// no instruction to find a 0 bit in a word. Therefore, the implementation of
// the bitmap is done by using 1 to indicate bits that are _not_ set and 0 for
// those which are set. Doing so, find the next non-set (per the high-level
// definition) bit can be optimized to use the BSF instruction (as it is
// equivalent to find the first bit set in a word).

void bitmap_reset(struct bitmap_t * const bitmap) {
    bitmap->free = bitmap->size;
    memset(bitmap->data, ~((uint8_t)0), BITMAP_WORD_COUNT(bitmap->size) * 4);
}

void bitmap_set_all(struct bitmap_t * const bitmap) {
    bitmap->free = 0;
    memzero(bitmap->data, BITMAP_WORD_COUNT(bitmap->size) * 4);
}

#include <bitmap.test>
