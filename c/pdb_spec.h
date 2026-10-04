/* pdb_spec.h - single source of truth for the pattern databases.
 * Shared by the host table generator, the tests and the solver core.
 * Freestanding: only <stdint.h>.
 *
 * Abstraction: track 4 cubies (position + twist each).
 *   entries = (7*6*5*4 arrangements) * 3^4 twists = 840 * 81 = 68,040
 * Dense index = arrangement * 81 + twists.
 * Packing: two 4-bit values per byte; EVEN index -> low nibble, ODD -> high.
 */
#ifndef PDB_SPEC_H
#define PDB_SPEC_H
#include <stdint.h>

#define PDB_ENTRIES 68040u
#define PDB_BYTES   34020u

/* Cubie sets (internal ids 0..6 = report cubies 1..7):
 * A = {1,2,4,5}, B = {3,5,6,7} in report numbering. */
#define A0 0
#define A1 1
#define A2 3
#define A3 4
#define B0 2
#define B1 4
#define B2 5
#define B3 6

#define X_HOME(c) ((uint8_t)((c) << 2))

/* Reference index: obvious loops, Horner form. */
static inline uint32_t pdb_index_ref(const uint8_t x[4])
{
    uint32_t perm = 0, ori = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t pos = (uint32_t)(x[i] >> 2), r = pos;
        for (int j = 0; j < i; j++) r -= ((uint32_t)(x[j] >> 2) < pos);
        perm = perm * (uint32_t)(7 - i) + r;
        ori = ori * 3u + (uint32_t)(x[i] & 3);
    }
    return perm * 81u + ori;
}

/* Fast index: comparisons (slt) and shift-add only, no multiply. */
static inline uint32_t pdb_index_fast(uint32_t x0, uint32_t x1,
                                      uint32_t x2, uint32_t x3)
{
    uint32_t a = x0 >> 2, b = x1 >> 2, c = x2 >> 2, d = x3 >> 2;
    uint32_t b1 = b - (b > a);
    uint32_t c1 = c - (c > a) - (c > b);
    uint32_t d1 = d - (d > a) - (d > b) - (d > c);
    uint32_t p = (a << 2) + (a << 1) + b1;     /* a*6 + b1 */
    p = (p << 2) + p + c1;                     /* *5 + c1  */
    p = (p << 2) + d1;                         /* *4 + d1  */
    uint32_t o = x0 & 3;
    o = (o << 1) + o + (x1 & 3);
    o = (o << 1) + o + (x2 & 3);
    o = (o << 1) + o + (x3 & 3);
    return (p << 6) + (p << 4) + p + o;        /* p*81 + o */
}

static inline uint32_t pdb_get(const uint8_t *tab, uint32_t i)
{
    return (uint32_t)(tab[i >> 1] >> ((i & 1u) << 2)) & 15u;
}

static inline void pdb_set(uint8_t *tab, uint32_t i, uint32_t v)
{
    uint32_t s = (i & 1u) << 2;
    tab[i >> 1] = (uint8_t)((tab[i >> 1] & ~(15u << s)) | (v << s));
}

#endif
