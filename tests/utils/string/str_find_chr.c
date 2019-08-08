#include <utils/string.h>
#include <stdio.h>

// Test the str_find_chr function.

#define SUCCESS do { return 0; } while (0);
#define FAILURE do { return 1; } while (0);

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("Failure at %s:%d\n", __FILE__, __LINE__); \
            FAILURE \
        }\
    } while(0)


int main(char argc, char **argv) {
    char const * const str1 = "ABCDE FGHIJ!";

    for (size_t i = 0; i < strlen(str1); ++i) {
        // We first test from index 0 and from index i. They should return i.
        TEST_ASSERT(str_find_chr(str1,str1[i],0) == i);
        TEST_ASSERT(str_find_chr(str1,str1[i],i) == i);
    }

    // Now try a real world example.
    TEST_ASSERT(str_find_chr(str1,' ',3) == 5);

    // Try a char that is not in the string.
    TEST_ASSERT(str_find_chr(str1,'@',0) == STR_NPOS);

    // Try to find in the string with a begin value that is bigger than the
    // string itself. This should return STR_NPOS as well.
    TEST_ASSERT(str_find_chr(str1,'@',strlen(str1)+10) == STR_NPOS);
}
