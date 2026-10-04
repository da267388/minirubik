/* node_stats.c - per-state operation counts for design analysis.
 * Build with -DOPCOUNT.  usage: node_stats FILE [cost_per_child] [budget]
 * FILE lines: "<14-char state> <exact distance>" (d11.txt, samples_d8_10.txt).
 * Verifies optimality of every answer, prints distribution of the counters
 * and a worst-case instruction estimate = max(children) * cost_per_child. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "solver_core.h"
#include "opcount.h"

#ifndef OPCOUNT
#error "build node_stats with -DOPCOUNT"
#endif

static int cmp(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s FILE [cost_per_child=200] [budget=50000000]\n", argv[0]); return 2; }
    double cost = argc > 2 ? atof(argv[2]) : 200.0;
    double budget = argc > 3 ? atof(argv[3]) : 5e7;
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    size_t cap = 4096, n = 0;
    uint64_t *ch = malloc(cap * sizeof *ch);
    uint64_t sum_ch = 0, sum_desc = 0, sum_hl = 0, sum_it = 0;
    uint64_t worst_ch = 0; char worst[16] = "";
    char line[64], in[16]; int dist, bad = 0;
    FILE *csv = getenv("NODE_CSV") ? fopen(getenv("NODE_CSV"), "w") : NULL;
    if (csv) fprintf(csv, "state,dist,len,iters,children,descended,hlook,idx\n");
    while (fgets(line, sizeof line, f)) {
        uint8_t path[12];
        if (sscanf(line, "%14s %d", in, &dist) != 2) continue;
        opc = (opc_t){0};
        int len = solve(in, path);
        if (len != dist) { if (bad++ < 10) printf("MISMATCH %s: len %d exact %d\n", in, len, dist); }
        if (n == cap) { cap *= 2; ch = realloc(ch, cap * sizeof *ch); }
        ch[n++] = opc.children;
        sum_ch += opc.children; sum_desc += opc.descended; sum_hl += opc.hlook; sum_it += opc.iters;
        if (opc.children > worst_ch) { worst_ch = opc.children; memcpy(worst, in, 15); }
        if (csv) fprintf(csv, "%s,%d,%d,%llu,%llu,%llu,%llu,%llu\n", in, dist, len,
                         (unsigned long long)opc.iters, (unsigned long long)opc.children,
                         (unsigned long long)opc.descended, (unsigned long long)opc.hlook,
                         (unsigned long long)opc.idx);
    }
    fclose(f);
    if (!n) { fprintf(stderr, "no states read\n"); return 2; }
    qsort(ch, n, sizeof *ch, cmp);
    printf("states: %zu   optimality mismatches: %d\n", n, bad);
    printf("children generated: mean %.0f  p50 %llu  p90 %llu  p99 %llu  max %llu (state %s)\n",
           (double)sum_ch / n, (unsigned long long)ch[n / 2], (unsigned long long)ch[n * 9 / 10],
           (unsigned long long)ch[n * 99 / 100], (unsigned long long)ch[n - 1], worst);
    printf("per state means: bounds tried %.2f, expanded %.0f, PDB lookups %.0f\n",
           (double)sum_it / n, (double)sum_desc / n, (double)sum_hl / n);
    printf("max/mean ratio: %.2f\n", (double)ch[n - 1] / ((double)sum_ch / n));
    printf("estimate at %.0f instr/child: worst %.3g instr vs budget %.3g -> %s (margin %.1fx)\n",
           cost, (double)ch[n - 1] * cost, budget,
           (double)ch[n - 1] * cost <= budget ? "WITHIN" : "OVER", budget / ((double)ch[n - 1] * cost));
    printf("max instr/child allowed by budget: %.0f\n", budget / (double)ch[n - 1]);
    return bad != 0;
}
