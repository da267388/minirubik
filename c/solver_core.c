/* solver_core.c - IDA* with two nibble-packed 4-cubie PDBs.
 *
 * State: x[c] = position<<2 | twist for each of the 7 cubies.
 *   one quarter turn  = 7 loads from QT (twist handled by the table: no mod 3)
 *   solved            = both PDB values are 0 (A u B covers all 7 cubies)
 * Search: explicit stack (<= 12 levels), no recursion, no heap.
 *
 * Build-time switches (default: SOLVER_OPT=1 = all optimizations on):
 *   OPT_INDEX  0: reference index (loops, multiply)   1: slt + shift-add
 *   OPT_UNROLL 0: loop over 7 cubies                  1: fully unrolled
 *   OPT_LAZY   0: h = max(hA,hB), one prune test      1: test A, then B lazily
 */
#include "solver_core.h"
#include "pdb_spec.h"
#include "tables.h"
#include "opcount.h"

#ifndef SOLVER_OPT
#define SOLVER_OPT 1
#endif
#ifndef OPT_INDEX
#define OPT_INDEX SOLVER_OPT
#endif
#ifndef OPT_UNROLL
#define OPT_UNROLL SOLVER_OPT
#endif
#ifndef OPT_LAZY
#define OPT_LAZY SOLVER_OPT
#endif

#ifdef OPCOUNT

opc_t opc = {
    .descended = 1
};
#endif

static const uint8_t move_face[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
static const uint8_t move_turn[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};

static inline uint32_t h_a(const uint8_t *x)
{
    uint32_t i;
    OPC(idx); OPC(hlook);
#if OPT_INDEX
    i = pdb_index_fast(x[A0], x[A1], x[A2], x[A3]);
#else
    { uint8_t t[4] = {x[A0], x[A1], x[A2], x[A3]}; i = pdb_index_ref(t); }
#endif
    return pdb_get(PDB_A_TAB, i);
}

static inline uint32_t h_b(const uint8_t *x)
{
    uint32_t i;
    OPC(idx); OPC(hlook);
#if OPT_INDEX
    i = pdb_index_fast(x[B0], x[B1], x[B2], x[B3]);
#else
    { uint8_t t[4] = {x[B0], x[B1], x[B2], x[B3]}; i = pdb_index_ref(t); }
#endif
    return pdb_get(PDB_B_TAB, i);
}

uint8_t solver_h_a(const uint8_t x[7]) { return (uint8_t)h_a(x); }
uint8_t solver_h_b(const uint8_t x[7]) { return (uint8_t)h_b(x); }
uint8_t solver_h(const uint8_t x[7])
{
    uint32_t a = h_a(x), b = h_b(x);
    return (uint8_t)(a > b ? a : b);
}
const uint8_t *solver_pdb(int which) { return which ? PDB_B_TAB : PDB_A_TAB; }
const uint8_t *solver_qt(void) { return QT; }

/* dst = one quarter turn of `face` applied to src (7 cubies). */
static inline void turn(uint8_t *dst, const uint8_t *src, uint32_t face)
{
    const uint8_t *q = QT + (face << 5);
    OPC(children);
#if OPT_UNROLL
    dst[0] = q[src[0]]; dst[1] = q[src[1]]; dst[2] = q[src[2]];
    dst[3] = q[src[3]]; dst[4] = q[src[4]]; dst[5] = q[src[5]];
    dst[6] = q[src[6]];
#else
    for (int c = 0; c < 7; c++) dst[c] = q[src[c]];
#endif
}

/* Parse into the cubie-indexed form. No division, no mod. */
static int parse(const char in[14], uint8_t x[8])
{
    uint32_t seen = 0, sum = 0;
    for (uint32_t i = 0; i < 7; i++) {
        uint32_t c = (uint32_t)(uint8_t)(in[i] - '1');
        uint32_t o = (uint32_t)(uint8_t)(in[7 + i] - '1');
        if (c > 6 || o > 2 || ((seen >> c) & 1u)) return SOLVE_E_INPUT;
        seen |= 1u << c;
        sum += o;
        x[c] = (uint8_t)(i << 2 | o);
    }
    while (sum >= 3) sum -= 3;
    return sum ? SOLVE_E_INPUT : 0;
}

/* Search state: st[d] = state at depth d; st[d+1] doubles as the "last
 * generated child" so the 3 turns of one face chain without recopying. */
static uint8_t st[13][8];
static uint8_t last_face[12], next_move[12], taken[12];

int solve(const char in[14], uint8_t path[12])
{
    int d;
    uint32_t bound, h0;
    if (parse(in, st[0]) < 0) return SOLVE_E_INPUT;
    {
        uint32_t a = h_a(st[0]), b = h_b(st[0]);
        h0 = a > b ? a : b;
    }
    if (h0 == 0) return 0;                       /* already solved */

    for (bound = h0; bound <= 11; bound++) {
        OPC(iters);
        d = 0; last_face[0] = 3; next_move[0] = 0;
        for (;;) {
            uint32_t m = next_move[d], f, g, ha, hb;
            if (m < 9 && move_face[m] == last_face[d]) m += 3;  /* same face */
            if (m >= 9) {                                       /* exhausted */
                if (d == 0) break;
                d--;
                continue;
            }
            next_move[d] = (uint8_t)(m + 1);
            f = move_face[m];
            turn(st[d + 1], move_turn[m] ? st[d + 1] : st[d], f);
            g = (uint32_t)d + 1;

#if OPT_LAZY
            ha = h_a(st[d + 1]);
            if (g + ha > bound) continue;
            hb = h_b(st[d + 1]);
            if (g + hb > bound) continue;
            if ((ha | hb) == 0) goto found;
#else
            ha = h_a(st[d + 1]);
            hb = h_b(st[d + 1]);
            if (hb > ha) ha = hb;
            if (g + ha > bound) continue;
            if (ha == 0) goto found;
#endif
            OPC(descended);
            taken[d] = (uint8_t)m;
            last_face[d + 1] = (uint8_t)f;
            next_move[d + 1] = 0;
            d++;
            continue;
        found:
            for (int i = 0; i < d; i++) path[i] = taken[i];
            path[d] = (uint8_t)m;
            return d + 1;
        }
    }
    return SOLVE_E_BOUND;
}
