#include <string/string.h>

// Test the streq function comparing two strings.

#define SUCCESS do { return 0; } while (0);
#define FAILURE do { return 1; } while (0);

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            FAILURE \
        }\
    } while(0)


int main(char argc, char **argv) {
    char const * const str1 = "Hello world!";
    char const * const str2 = "Another string";
    char const * const str3 = "";
    char const * const str4 = "Hello world!";

    TEST_ASSERT(streq(str1,str1));
    TEST_ASSERT(streq(str2,str2));
    TEST_ASSERT(streq(str3,str3));
    TEST_ASSERT(streq(str1,str4));
    TEST_ASSERT(streq(str4,str1));

    TEST_ASSERT(!streq(str1,str2));
    TEST_ASSERT(!streq(str1,str3));
    TEST_ASSERT(!streq(str2,str3));

    TEST_ASSERT(!streq(str2,str1));
    TEST_ASSERT(!streq(str3,str1));
    TEST_ASSERT(!streq(str3,str2));
}
