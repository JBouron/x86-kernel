#pragma once
#include <fs.h>
#include <proc.h>

// ELF loading functions. For now only Executable ELFs can be loaded, dynamic
// linking is not yet supported.

// Load an ELF binary into a process' address space and update the EIP of the
// process to point to the entry point specified by the ELF.
// @param path: File path to the ELF to be loaded.
// @param proc: The proc in which the ELF should be loaded.
// @return: true if the ELF binary has successfully been loaded into the
// process' address space otherwise. false otherwise. If this function returns
// false the content of the process' address space is undefined (some sections
// could have been mapped while some are missing).
bool load_elf_binary(struct file * const file, struct proc * const proc);

// Execute ELF related tests.
void elf_test(void);
