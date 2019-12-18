/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start_kernel)
 
/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	. = 0x100000;
 
	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
    __KERNEL_START = .;
	.text ALIGN(4K) : AT(ADDR(.text))
	{
        __SECTION_TEXT_START = .;
		*(.multiboot)
		*(.text)
        __SECTION_TEXT_END = .;
	}
 
	/* Read-only data. */
	.rodata ALIGN(4K) : AT(ADDR(.rodata))
	{
        __SECTION_RODATA_START = .;
		*(.rodata)
        __SECTION_RODATA_END = .;
	}
 
	/* Read-write data (initialized) */
	.data ALIGN(4K) : AT(ADDR(.data))
	{
        __SECTION_DATA_START = .;
		*(.data)
        __SECTION_DATA_END = .;
	}
 
	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K) : AT(ADDR(.bss))
	{
        __SECTION_BSS_START = .;
		*(COMMON)
		*(.bss)
        __SECTION_BSS_END = .;
	}
    __KERNEL_END = .;
 
	/* The compiler may produce other sections, by default it will put them
	 * in a segment with the same name. Simply add stuff here as needed. */
}
