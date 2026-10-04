/* H1: admissibility over ALL 3,674,160 states, using the solver's own h().
 *   (a) hA <= d, hB <= d, max <= d for every state
 *   (b) stronger: each table entry equals the MINIMUM true distance over its
 *       fiber (computed independently from dist.bin with the reference index)
 * Also prints heuristic-quality statistics for design analysis. */
#include "testutil.h"
#include "pdb_spec.h"
#include "solver_core.h"

int main(int argc, char **argv)
{
    uint8_t *dist = load_dist(argc > 1 ? argv[1] : "build/dist.bin");
    static uint8_t minf[2][PDB_ENTRIES];
    static uint32_t gap[16];            /* d - h histogram */
    static const uint8_t setA[4] = {A0, A1, A2, A3}, setB[4] = {B0, B1, B2, B3};
    uint64_t sum_h = 0, sum_d = 0, tight = 0, viol = 0;
    memset(minf, 0xFF, sizeof minf);
    for (uint32_t r = 0; r < STATES; r++) {
        state_t s; uint8_t x[7], ta[4], tb[4];
        unrank_state(r, &s); cube_to_x(&s, x);
        uint32_t d = dist[r], ha = solver_h_a(x), hb = solver_h_b(x), h = solver_h(x);
        if (ha > d || hb > d || h > d) { if (viol++ < 5) printf("H1 VIOLATION rank %u: d=%u hA=%u hB=%u\n", r, d, ha, hb); }
        for (int i = 0; i < 4; i++) { ta[i] = x[setA[i]]; tb[i] = x[setB[i]]; }
        uint32_t ia = pdb_index_ref(ta), ib = pdb_index_ref(tb);
        if (d < minf[0][ia]) minf[0][ia] = (uint8_t)d;
        if (d < minf[1][ib]) minf[1][ib] = (uint8_t)d;
        sum_h += h; sum_d += d; tight += (h == d);
        gap[d - (h > d ? d : h)]++;
    }
    for (int t = 0; t < 2; t++)
        for (uint32_t i = 0; i < PDB_ENTRIES; i++)
            if (pdb_get(solver_pdb(t), i) != minf[t][i]) {
                if (viol++ < 10) printf("H1 FIBER-MIN mismatch table %c idx %u: table=%u min=%u\n", 'A' + t, i, pdb_get(solver_pdb(t), i), minf[t][i]);
            }
    printf("mean d = %.3f, mean h = %.3f, h == d for %.2f%% of states\n",
           (double)sum_d / STATES, (double)sum_h / STATES, 100.0 * (double)tight / STATES);
    printf("d - h histogram:");
    for (int g = 0; g < 12; g++) printf(" %d:%u", g, gap[g]);
    printf("\n");
    printf(viol ? "H1 FAIL (%llu problems)\n" : "H1 PASS: h <= d everywhere; tables equal fiber minima\n", (unsigned long long)viol);
    return viol != 0;
}
