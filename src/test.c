#include <test.h>
#include <debug.h>

void __run_single_test(test_function const func, char const * const name) {
    bool const res = func();
    char const * const str = res ? " OK " : "FAIL";
    LOG("[%s] %s\n", str, name);
}
