#include <utils/string.h>

// Test the strneq function comparing two strings.

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

    // Whole comparisons.
    TEST_ASSERT(strneq(str1,str1,strlen(str1)));
    TEST_ASSERT(strneq(str2,str2,strlen(str2)));
    TEST_ASSERT(strneq(str3,str3,strlen(str3)));
    TEST_ASSERT(strneq(str1,str4,strlen(str1)));
    TEST_ASSERT(strneq(str4,str1,strlen(str1)));

    // Whole comparisons.
    TEST_ASSERT(!strneq(str1,str2,strlen(str1)));
    TEST_ASSERT(strneq(str1,str3,strlen(str3)));
    TEST_ASSERT(strneq(str2,str3,strlen(str3)));

    // Whole comparisons.
    TEST_ASSERT(!strneq(str2,str1,strlen(str1)));
    TEST_ASSERT(strneq(str3,str1,strlen(str3)));
    TEST_ASSERT(strneq(str3,str2,strlen(str3)));

    // Now check actual substrings.
    // Check the substring "world!" in str{1,4}.
    TEST_ASSERT(strneq(str1+6,str4+6,6));
    // Substring " " in str{1,2}.
    TEST_ASSERT(strneq(str1+5,str2+7,1));
}
