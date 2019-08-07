#ifndef _UTILS_KERNEL_MAP_H
#define _UTILS_KERNEL_MAP_H

// This file contains the memory locations of all the section of the kernel's
// executable.

// The following symbols are defined in the linker.ld script. Those are not
// meant to be used directly. Use the macros defined down below to get the
// addresses.
extern uint8_t __KERNEL_START;
extern uint8_t __SECTION_TEXT_START;
extern uint8_t __SECTION_TEXT_END;
extern uint8_t __SECTION_RODATA_START;
extern uint8_t __SECTION_RODATA_END;
extern uint8_t __SECTION_DATA_START;
extern uint8_t __SECTION_DATA_END;
extern uint8_t __SECTION_BSS_START;
extern uint8_t __SECTION_BSS_END;
extern uint8_t __KERNEL_END;

// The start and end addresses of the kernel's section as well as the kernel
// itself.
#define KERNEL_START            (&__KERNEL_START)
#define SECTION_TEXT_START      (&__SECTION_TEXT_START)
#define SECTION_TEXT_END        (&__SECTION_TEXT_END)
#define SECTION_RODATA_START    (&__SECTION_RODATA_START)
#define SECTION_RODATA_END      (&__SECTION_RODATA_END)
#define SECTION_DATA_START      (&__SECTION_DATA_START)
#define SECTION_DATA_END        (&__SECTION_DATA_END)
#define SECTION_BSS_START       (&__SECTION_BSS_START)
#define SECTION_BSS_END         (&__SECTION_BSS_END)
#define KERNEL_END              (&__KERNEL_END)

// The size of the kernel's sections and the kernel itself.
#define KERNEL_SIZE         (KERNEL_END - KERNEL_START)
#define SECTION_TEXT_SIZE   (SECTION_TEXT_END - SECTION_TEXT_START)
#define SECTION_RODATA_SIZE (SECTION_RODATA_END - SECTION_RODATA_START)
#define SECTION_DATA_SIZE   (SECTION_DATA_END - SECTION_DATA_START)
#define SECTION_BSS_SIZE    (SECTION_BSS_END - SECTION_BSS_START)

#endif
