#include <stdio.h>
#include <assert.h>

#include "hashmap.h"

#define testing(fn) printf(#fn); fn(); printf("[OK]\n");

void test_hmap_init(void) {
    struct hmap hmap;
    hmap_init(&hmap);
    assert(hmap.crash == 0);
}

void test_hmap_insert_get(void) {
    struct hmap hmap;
    char *key = "hello", *val = "world";
    assert(hmap_insert(&hmap, key, val) == 1);
    assert(hmap_get(&hmap, key) != 0);
    assert(!strcmp(hmap_get(&hmap, key), val));

    assert(hmap_insert(&hmap,val, key) == 1);
    assert(hmap_get(&hmap, val) != 0);
    assert(!strcmp(hmap_get(&hmap, val), key));

    assert(!strcmp(hmap_get(&hmap, key), val));
}

int main(int argc, const char **argv) {
    testing(test_hmap_init);
    testing(test_hmap_insert_get);
    return 0;
}

