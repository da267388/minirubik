/* H3: the solver returns a path of length == exact distance, and the path
 * really solves the state (checked with the independent cube model).
 * usage: h3_optimal [dist.bin] [stride] [part] [nparts]
 *   checks ranks r = (part + k*nparts) * stride, k = 0,1,2,...  (stride 1 and
 *   nparts N, part 0..N-1 together cover the whole domain). */
#include "testutil.h"
#include "solver_core.h"

int main(int argc, char **argv)
{
    uint8_t *dist = load_dist(argc > 1 ? argv[1] : "build/dist.bin");
    uint32_t stride = argc > 2 ? (uint32_t)atoi(argv[2]) : 1;
    uint32_t part = argc > 3 ? (uint32_t)atoi(argv[3]) : 0;
    uint32_t nparts = argc > 4 ? (uint32_t)atoi(argv[4]) : 1;
    uint64_t checked = 0, bad = 0;
    double t0 = now_s();
    for (uint64_t k = part;; k += nparts) {
        uint64_t r = k * stride;
        if (r >= STATES) break;
        state_t s; char in[15]; uint8_t path[12];
        unrank_state((uint32_t)r, &s);
        format_state(&s, in);
        int n = solve(in, path);
        int ok = n == dist[r];
        if (ok) {
            for (int i = 0; i < n; i++) s = apply_move(s, path[i]);
            ok = is_solved(&s);
        }
        if (!ok && bad++ < 10) printf("H3 FAIL rank %llu (%s): len %d, exact %u\n", (unsigned long long)r, in, n, dist[r]);
        checked++;
    }
    printf("H3 part %u/%u: checked %llu states, %llu failures, %.1f s\n", part, nparts,
           (unsigned long long)checked, (unsigned long long)bad, now_s() - t0);
    return bad != 0;
}
