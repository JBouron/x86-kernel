// GDT operations.
#ifndef _MEMORY_GDT_H
#define _MEMORY_GDT_H
// Initialize the GDT to get a flat model and refresh all segment register to
// use the new segments.
void
gdt_init(void);
#endif
