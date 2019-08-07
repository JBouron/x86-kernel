#include <boot/cmdline/cmdline.h>
#include <utils/string.h>

struct cmdline_params_t CMDLINE_PARAMS;

// Initializes the values of CMDLINE_PARAMS to their default.
static void
__init_cmdline_params(void) {
    CMDLINE_PARAMS.wait_start = 0;
}

static void
__parse_param(char const * const param, size_t const len) {
    // For now the parsing is *very* primitive since we only handle one
    // parameter: the --wait. Thus a simple string comparison should be enough.
    if (strneq(param, "--wait", len)) {
        CMDLINE_PARAMS.wait_start = 1;
    }
}

void
cmdline_parse(char const * const cmdline) {
    // Start by initializing the struct.
    __init_cmdline_params();

    // We can now parse the string to fill the CMDLINE_PARAMS struct to the
    // correct values.
    size_t old_pos = 0;
    size_t curr_pos = 0;

    while ((curr_pos = str_find_chr(cmdline, ' ', old_pos)) != STR_NPOS) {
        // We found a param starting at old_pos and ending at curr_pos, process
        // it.
        char const * const param = cmdline + old_pos;
        size_t const len = curr_pos - old_pos - 1;
        old_pos = curr_pos + 1;

        __parse_param(param, len);
    }

    // The above loop will not process the last param (since it does not stop
    // with a space char) thus handle this special case here.
    char const * const param = cmdline + old_pos;
    size_t const len = strlen(param);
    __parse_param(param, len);
}
