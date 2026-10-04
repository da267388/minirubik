#ifndef TESTUTIL_H
#define TESTUTIL_H
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cube_model.h"

static inline uint8_t *load_dist(const char *path)
{
    uint8_t *d = malloc(STATES);
    FILE *f = fopen(path, "rb");
    if (!f || fread(d, 1, STATES, f) != STATES) {
        fprintf(stderr, "cannot read %s (run `make oracle` first)\n", path);
        exit(2);
    }
    fclose(f);
    return d;
}
static inline double now_s(void) { return (double)clock() / CLOCKS_PER_SEC; }
#endif
