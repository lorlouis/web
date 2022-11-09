#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H 1

#define __XSTR(s) __STR(s)
#define __STR(s) #s

struct {
    int line;
    const char *func;
    const char *file;
    char *assertion;
} test_failed_data;

#define assert(assert) do { \
    if(!(assert)) { \
        test_failed_data.line = __LINE__; \
        test_failed_data.func = __FUNCTION__; \
        test_failed_data.file = __FILE__; \
        test_failed_data.assertion = #assert; \
        goto cleanup; \
    } \
} while(0)

#endif
