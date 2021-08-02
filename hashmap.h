#include <stddef.h>
#include <string.h>
#include <assert.h>

#define HASHMAP_ELEM_TEXT_BUFFER_LEN 64

#define HASHMAP_ELEM_CAPACITY 2560

struct hmap_elem {
    char key[HASHMAP_ELEM_TEXT_BUFFER_LEN];
    char value[HASHMAP_ELEM_TEXT_BUFFER_LEN];
};

struct hmap {
    int crash;
    struct hmap_elem elems[HASHMAP_ELEM_CAPACITY];
};

static inline long sumstr(const char *data) {
    long sum = 0;
    while(*data != '\0') {
        sum += *data;
        data++;
    }
    return sum;
}

static inline struct hmap* hmap_init(struct hmap *hmap) {
    hmap->crash = 0;
    return hmap;
}

static inline size_t hash_to_range(size_t value, size_t max_value) {
/* https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that
 * -the-world-forgot-or-a-better-alternative-to-integer-modulo/ */
    return (value * 11400714819323198485llu) % (max_value);
}

static inline int hmap_insert(struct hmap * hmap, const char *key, const char* value) {
    size_t hash = sumstr(key);
    size_t pos = hash_to_range(hash, HASHMAP_ELEM_CAPACITY);
    size_t index = pos--;
    while(index % HASHMAP_ELEM_CAPACITY != pos
            && *hmap->elems[index % HASHMAP_ELEM_CAPACITY].key != '\0'
            && strncmp(key, hmap->elems[index % HASHMAP_ELEM_CAPACITY].key,
                HASHMAP_ELEM_TEXT_BUFFER_LEN)) {
        index++;
    }
    if(index % HASHMAP_ELEM_CAPACITY == pos) return 0;

    struct hmap_elem *elem = &hmap->elems[index % HASHMAP_ELEM_CAPACITY];
    strncpy(elem->key, key, HASHMAP_ELEM_TEXT_BUFFER_LEN-1);
    elem->key[HASHMAP_ELEM_TEXT_BUFFER_LEN-1] = '\0';

    strncpy(elem->value, value, HASHMAP_ELEM_TEXT_BUFFER_LEN-1);
    elem->value[HASHMAP_ELEM_TEXT_BUFFER_LEN-1] = '\0';
    return 1;
}
static inline char* hmap_get(struct hmap *hmap, const char *key) {

    size_t hash = sumstr(key);
    size_t pos = hash_to_range(hash, HASHMAP_ELEM_CAPACITY);
    struct hmap_elem *elem = hmap->elems + pos;

    if(*elem->key == '\0') return 0;
    size_t index = pos--;
    while(index % HASHMAP_ELEM_CAPACITY != pos
            && strncmp(key, hmap->elems[index % HASHMAP_ELEM_CAPACITY].key,
                HASHMAP_ELEM_TEXT_BUFFER_LEN)) {
        index++;
    }
    if(index % HASHMAP_ELEM_CAPACITY == pos) return 0;

    elem = &hmap->elems[index % HASHMAP_ELEM_CAPACITY];
    return elem->value;
}


static inline int hmap_delete(struct hmap * hmap, const char *key) {
    size_t hash = sumstr(key);
    size_t pos = hash_to_range(hash, HASHMAP_ELEM_CAPACITY);
    struct hmap_elem *elem = hmap->elems + pos;
    if(*elem->key == '\0') return 0;

    size_t index = pos--;
    while(index % HASHMAP_ELEM_CAPACITY != pos
            && strncmp(key, hmap->elems[index % HASHMAP_ELEM_CAPACITY].key,
                HASHMAP_ELEM_TEXT_BUFFER_LEN)) {
        index++;
    }
    if(index % HASHMAP_ELEM_CAPACITY == pos) return 0;
    elem = &hmap->elems[index % HASHMAP_ELEM_CAPACITY];
    *elem->value = '\0';
    return 1;
}
