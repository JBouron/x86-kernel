#pragma once
#include <utils/types.h>

// Contains a summary of the parameters' value passed through the command line.
struct cmdline_params {
    // The kernel can wait for the debugger before executing the kernel_main
    // function.
    uint8_t wait_start;
};

// Global variable containing the content of the command-line at boot time.
extern struct cmdline_params CMDLINE_PARAMS;

// Parse a command-line an set the flags in the global variable CMDLINE_PARAMS.
void
cmdline_parse(char const * const cmdline);

