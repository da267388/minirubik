/* pdb_build.h - HOST ONLY. Builds the quarter-turn table and abstract BFS. */
#ifndef PDB_BUILD_H
#define PDB_BUILD_H
#include <stdlib.h>
#include "cube_model.h"
#include "pdb_spec.h"

/* QT[face*32 + x] = x after one quarter turn of `face`; 0xFF = invalid x. */
static inline void build_qt(uint8_t qt[96])
{
    memset(qt, 0xFF, 96);
    for (int f = 0; f < 3; f++)
        for (int j = 0; j < CUBIES; j++)
            for (int t = 0; t < 3; t++) {
                int dst = -1;
                for (int i = 0; i < CUBIES; i++) if (source[f][i] == j) dst = i;
                qt[f * 32 + (j << 2 | t)] =
                    (uint8_t)(dst << 2 | ((t + twist[f][dst]) % 3));
            }
}

/* Exact abstract distances by BFS in the abstract space (reference index).
 * dist[] has PDB_ENTRIES bytes; returns number of entries reached. */
static inline uint32_t pdb_bfs(const uint8_t qt[96], const uint8_t set[4],
                               uint8_t *dist)
{
    uint32_t *queue = malloc(PDB_ENTRIES * sizeof *queue);
    uint32_t head = 0, tail = 0;
    uint8_t g[4];
    memset(dist, 0xFF, PDB_ENTRIES);
    for (int i = 0; i < 4; i++) g[i] = X_HOME(set[i]);
    dist[pdb_index_ref(g)] = 0;
    queue[tail++] = (uint32_t)g[0] | (uint32_t)g[1] << 8 | (uint32_t)g[2] << 16 |
                    (uint32_t)g[3] << 24;
    while (head < tail) {
        uint32_t q = queue[head++];
        uint8_t x[4];
        for (int i = 0; i < 4; i++) x[i] = (uint8_t)(q >> (8 * i));
        uint8_t d = dist[pdb_index_ref(x)];
        for (int f = 0; f < 3; f++) {
            uint8_t y[4] = {x[0], x[1], x[2], x[3]};
            for (int k = 0; k < 3; k++) {
                for (int i = 0; i < 4; i++) y[i] = qt[f * 32 + y[i]];
                uint32_t idx = pdb_index_ref(y);
                if (dist[idx] == 0xFF) {
                    dist[idx] = (uint8_t)(d + 1);
                    queue[tail++] = (uint32_t)y[0] | (uint32_t)y[1] << 8 |
                                    (uint32_t)y[2] << 16 | (uint32_t)y[3] << 24;
                }
            }
        }
    }
    free(queue);
    return tail;
}
#endif
