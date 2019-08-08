#include <utils/string.h>

// Test the string manipulation functions defined under src/string/*.

int main(char argc, char **argv) {
    char const * const test_string = "Hello world!";
    if (strlen(test_string) == 12) {
        return 0;
    } else {
        return 1;
    }
}
