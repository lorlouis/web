#include <stdio.h>
#include <string.h>

#include "test_assert.h"

extern int TOTAL_TESTS;

#define RUN_TEST(fn) \
    memset(&test_failed_data, 0, sizeof(test_failed_data)); \
    fprintf(stderr, "%d/%d\t", __COUNTER__ + 1, TOTAL_TESTS); \
    failed += run_test(fn, #fn); \
    if(failed && failfast) goto end;

static int run_test(void (*fn)(void), char *name) {
    fn();
    /* allow for passing tests to be ignore with 1>/dev/null */
    if(test_failed_data.assertion) {
        fprintf(stderr, "running %-32s ", name);
        fprintf(stderr,
                "[FAIL]\n  %s:%d `(%s)!=true`\n",
                test_failed_data.file,
                test_failed_data.line,
                test_failed_data.assertion);
        return 1;
    }
    printf("running %-32s %s\n", name, "[ OK ]");
    return 0;
}

#include "tests.c"

int main(int argc, const char **argv) {
    int failed = 0;
    _Bool failfast = 0;

    for(int i = 1; i < argc; i++) {
        size_t arg_len = strlen(argv[i]);
        if(sizeof("--failfast") == arg_len && strncmp(argv[i], "--failfast", arg_len)) {
            failfast = 1;
            continue;
        }
    }
    puts("RUNNING TESTS\n");
    /* ADD TESTS HERE */
    RUN_TEST(test_ky_split);

    /* END OF TESTS */
    printf("REPORT:\n\tfailed: %d\tpassed: %d\n", failed, TOTAL_TESTS-failed);
end:
    return failed;
}

int TOTAL_TESTS = __COUNTER__;
