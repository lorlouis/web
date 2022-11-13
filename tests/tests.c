#include <string.h>
#include "test_assert.h"

#include "../src/config.c"

void test_ky_split(void) {
    {
    char line[] = "hello =  \"world !\"";
    char *key;
    char *value;
    int ret = key_value_split(line, &key, &value);
    assert(ret == 0);
    assert(!strcmp(key, "hello"));
    assert(!strcmp(value, "world !"));
    }

    {
    char line[] = "hello =  \".\"";
    char *key;
    char *value;
    int ret = key_value_split(line, &key, &value);
    assert(ret == 0);
    assert(!strcmp(key, "hello"));
    assert(!strcmp(value, "."));
    }
cleanup:;
}
