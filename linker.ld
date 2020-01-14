/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start_kernel)
 
/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
    /* The kernel is a higher-half kernel. Starting at >3GiB. */
	. = 0xC0000000;
    KERNEL_PHY_OFFSET = .;

    /* Start 1MB above the 3GiB mark, the reason is to make the sections
     * placement easier as we will only need to substract 0xC0000000 to get
     * above the 1MB mark. */
	. = . + 0x100000;
 
	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
    KERNEL_START = .;
	.text ALIGN(4K) : AT(ADDR(.text) - KERNEL_PHY_OFFSET)
	{
        SECTION_TEXT_START = .;
		*(.multiboot)
		*(.text)
        SECTION_TEXT_END = .;
	}
 
	/* Read-only data. */
	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_PHY_OFFSET)
	{
        SECTION_RODATA_START = .;
		*(.rodata)
        SECTION_RODATA_END = .;
	}
 
	/* Read-write data (initialized) */
	.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_PHY_OFFSET)
	{
        SECTION_DATA_START = .;
		*(.data)
        SECTION_DATA_END = .;
	}
 
	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_PHY_OFFSET)
	{
        SECTION_BSS_START = .;
		*(COMMON)
		*(.bss)
        SECTION_BSS_END = .;
	}
    KERNEL_END = .;
 
	/* The compiler may produce other sections, by default it will put them
	 * in a segment with the same name. Simply add stuff here as needed. */
}
