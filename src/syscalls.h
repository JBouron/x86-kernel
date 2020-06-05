#pragma once
#include <types.h>

// Exit the current process. This function will obviously not return.
// @param exit_code: The value of the exit code.
void do_exit(uint8_t const exit_code);
