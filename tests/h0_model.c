/* H0: model sanity (cheap, run first).
 *  - four quarter turns of every face = identity
 *  - rank/unrank is a bijection on all 3,674,160 states
 *  - the solver's QT table agrees with the model on every state and face
 *  - QT: every face is a permutation of the 21 valid x values, order 4 */
#include "testutil.h"
#include "pdb_spec.h"
#include "solver_core.h"

int main(void)
{
    int fail = 0;
    const uint8_t *qt = solver_qt();
    for (int f = 0; f < 3; f++) {
        state_t s = solved_state();
        for (int k = 0; k < 4; k++) s = quarter(&s, f);
        state_t id = solved_state();
        if (memcmp(&s, &id, sizeof s)) { printf("H0 FAIL: face %d order\n", f); fail++; }
        uint8_t seen[32] = {0};
        for (int j = 0; j < 7; j++) for (int t = 0; t < 3; t++) {
            uint8_t x = (uint8_t)(j << 2 | t), y = qt[f * 32 + x], z = x;
            if (y >= 32 || seen[y]++) { printf("H0 FAIL: QT face %d not a permutation\n", f); fail++; }
            for (int k = 0; k < 4; k++) z = qt[f * 32 + z];
            if (z != x) { printf("H0 FAIL: QT face %d order\n", f); fail++; }
        }
    }
    for (uint32_t r = 0; r < STATES && fail < 10; r++) {
        state_t s; unrank_state(r, &s);
        if (rank_state(&s) != r) { printf("H0 FAIL: rank/unrank at %u\n", r); fail++; break; }
        uint8_t x[7], y[7];
        cube_to_x(&s, x);
        for (int f = 0; f < 3; f++) {
            state_t t = quarter(&s, f);
            cube_to_x(&t, y);
            for (int c = 0; c < 7; c++)
                if (qt[f * 32 + x[c]] != y[c]) { printf("H0 FAIL: QT vs model r=%u f=%d c=%d\n", r, f, c); fail++; r = STATES; break; }
        }
    }
    printf(fail ? "H0 FAIL\n" : "H0 PASS: model, rank/unrank, QT consistent\n");
    return fail != 0;
}
