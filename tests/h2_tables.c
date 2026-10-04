/* H2: tables fully populated.  For each PDB: every value <= 11 (diameter),
 * no 15 sentinel, exactly one zero (the solved entry), solved entry is 0,
 * and the maximum value is reported.  Also reports static data size. */
#include "testutil.h"
#include "pdb_spec.h"
#include "solver_core.h"

int main(void)
{
    int fail = 0;
    static const uint8_t setA[4] = {A0, A1, A2, A3}, setB[4] = {B0, B1, B2, B3};
    for (int t = 0; t < 2; t++) {
        const uint8_t *tab = solver_pdb(t);
        uint32_t hist[16] = {0}, max = 0;
        uint8_t home[4];
        for (int i = 0; i < 4; i++) home[i] = X_HOME((t ? setB : setA)[i]);
        for (uint32_t i = 0; i < PDB_ENTRIES; i++) {
            uint32_t v = pdb_get(tab, i);
            hist[v]++; if (v > max) max = v;
        }
        if (hist[15] || max > 11) { printf("H2 FAIL: table %c has value %u / sentinel\n", 'A' + t, max); fail++; }
        if (hist[0] != 1) { printf("H2 FAIL: table %c has %u zero entries\n", 'A' + t, hist[0]); fail++; }
        if (pdb_get(tab, pdb_index_ref(home)) != 0) { printf("H2 FAIL: table %c solved entry != 0\n", 'A' + t); fail++; }
        printf("table %c: max=%u histogram:", 'A' + t, max);
        for (int d = 0; d <= (int)max; d++) printf(" %d:%u", d, hist[d]);
        printf("\n");
    }
    printf("static data: PDB 2 x %u + QT 96 + move tables 18 = %u bytes (limit 131072)\n",
           PDB_BYTES, 2 * PDB_BYTES + 96 + 18);
    printf(fail ? "H2 FAIL\n" : "H2 PASS: tables fully populated, solved entries are 0\n");
    return fail != 0;
}
