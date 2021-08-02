#include "mimes.h"
#include "stdio.h"

const int NB_MIMES = sizeof(mimes) / sizeof(char*[2]);

void build_mimes_hmap(struct hmap *hmap) {
    const char *(*walk)[2] = (const char *(*)[2])mimes;
    for(int i = 0; i < NB_MIMES; i++) {
        if(!hmap_insert(hmap, walk[i][0], walk[i][1])) {
            printf("Could not insert {%s: %s}\n", walk[i][0], walk[i][1]);
        }
    }
}
