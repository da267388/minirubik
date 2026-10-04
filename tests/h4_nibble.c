/* H4: packed accessor agrees with an unpacked reference at EVEN and ODD
 * indices, and the fast index function equals the reference index on all
 * 68,040 valid tuples (and is a bijection onto [0, 68040)). */
#include "testutil.h"
#include "pdb_build.h"
#include "solver_core.h"

int main(void)
{
    int fail = 0;
    static const uint8_t setA[4] = {A0, A1, A2, A3}, setB[4] = {B0, B1, B2, B3};
    uint8_t qt[96];
    static uint8_t ref[PDB_ENTRIES], seen[PDB_ENTRIES];
    build_qt(qt);
    for (int t = 0; t < 2; t++) {
        const uint8_t *tab = solver_pdb(t);
        uint32_t n = pdb_bfs(qt, t ? setB : setA, ref), even = 0, odd = 0;
        if (n != PDB_ENTRIES) { printf("H4 FAIL: reference table incomplete\n"); fail++; }
        for (uint32_t i = 0; i < PDB_ENTRIES; i++) {
            if (pdb_get(tab, i) != ref[i]) { if (fail++ < 10) printf("H4 FAIL: table %c idx %u (%s): %u != %u\n", 'A' + t, i, i & 1 ? "odd" : "even", pdb_get(tab, i), ref[i]); }
            if (i & 1) odd++; else even++;
        }
        printf("table %c: compared %u even + %u odd indices\n", 'A' + t, even, odd);
    }
    /* pdb_set / pdb_get round trip on both parities */
    { uint8_t b[2] = {0, 0};
      pdb_set(b, 0, 9); pdb_set(b, 1, 5); pdb_set(b, 2, 15); pdb_set(b, 3, 1);
      if (pdb_get(b, 0) != 9 || pdb_get(b, 1) != 5 || pdb_get(b, 2) != 15 || pdb_get(b, 3) != 1) { printf("H4 FAIL: set/get\n"); fail++; } }
    /* index functions over every valid tuple */
    uint32_t count = 0;
    for (int a = 0; a < 7; a++) for (int b = 0; b < 7; b++) for (int c = 0; c < 7; c++) for (int d = 0; d < 7; d++) {
        if (a == b || a == c || a == d || b == c || b == d || c == d) continue;
        for (int o = 0; o < 81; o++) {
            uint8_t x[4] = {(uint8_t)(a << 2 | o / 27), (uint8_t)(b << 2 | o / 9 % 3),
                            (uint8_t)(c << 2 | o / 3 % 3), (uint8_t)(d << 2 | o % 3)};
            uint32_t r = pdb_index_ref(x), f = pdb_index_fast(x[0], x[1], x[2], x[3]);
            if (r != f || f >= PDB_ENTRIES || seen[f]++) { if (fail++ < 10) printf("H4 FAIL: index mismatch/collision (ref %u, fast %u)\n", r, f); }
            count++;
        }
    }
    printf("index: %u tuples, fast == ref, bijective\n", count);
    printf(fail ? "H4 FAIL\n" : "H4 PASS: nibble accessor and index functions correct\n");
    return fail != 0;
}
