/* pdb_select.c - HOST ONLY design-space explorer (needs dist.bin).
 *   pdb_select sweep [dist.bin]
 *       all 35 four-cubie subsets: mean h; best pairs; best third table.
 *   pdb_select eval MASKS [d11.txt] [dist.bin]
 *       MASKS = comma list of hex bit masks over cubies 1..7 (bit0 = cubie 1),
 *       e.g. 1b,74 = {1,2,4,5},{3,5,6}... ; reports mean h and IDA* node
 *       counts (children generated) over the states in the file.
 * Tables here are fiber minima computed straight from dist.bin, i.e. an
 * independent construction from gen_tables' abstract BFS. */
#include <stdlib.h>
#include "testutil.h"
#include "pdb_build.h"

#define MAXT 8
static uint8_t *dist;
static uint8_t (*XS)[8];
static int nsub, submask[40];
static uint8_t (*tab)[PDB_ENTRIES];     /* tab[subset][index] */
static uint8_t qt[96];

static uint32_t sub_index(int s, const uint8_t *x)
{
    uint8_t t[4]; int k = 0;
    for (int c = 0; c < 7; c++) if (submask[s] >> c & 1) t[k++] = x[c];
    return pdb_index_ref(t);
}

static void build_sub(int s)
{
    memset(tab[s], 0xFF, PDB_ENTRIES);
    for (uint32_t r = 0; r < STATES; r++) {
        uint32_t i = sub_index(s, XS[r]);
        if (dist[r] < tab[s][i]) tab[s][i] = dist[r];
    }
}

/* ---- node counting with an arbitrary list of tables (recursion is fine on host) ---- */
static int use[MAXT], nuse;
static uint64_t children;
static int hmax(const uint8_t *x)
{
    int h = 0;
    for (int i = 0; i < nuse; i++) { int v = tab[use[i]][sub_index(use[i], x)]; if (v > h) h = v; }
    return h;
}
static int solved_x(const uint8_t *x) { for (int c = 0; c < 7; c++) if (x[c] != (c << 2)) return 0; return 1; }
static int dfs(const uint8_t *x, int g, int bound, int last)
{
    for (int f = 0; f < 3; f++) {
        if (f == last) continue;
        uint8_t y[8]; memcpy(y, x, 8);
        for (int t = 0; t < 3; t++) {
            for (int c = 0; c < 7; c++) y[c] = qt[f * 32 + y[c]];
            children++;
            int h = hmax(y);
            if (g + 1 + h > bound) continue;
            if (solved_x(y)) return 1;
            if (dfs(y, g + 1, bound, f)) return 1;
        }
    }
    return 0;
}
static int cmpu(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }

static void eval_nodes(const char *file)
{
    FILE *f = fopen(file, "r"); char line[64], in[16]; int d;
    if (!f) { perror(file); exit(2); }
    size_t n = 0, cap = 4096; uint64_t *v = malloc(cap * sizeof *v), sum = 0; int bad = 0;
    while (fgets(line, sizeof line, f)) {
        state_t s; uint8_t x[8] = {0};
        if (sscanf(line, "%14s %d", in, &d) != 2 || !parse_state_model(in, &s)) continue;
        cube_to_x(&s, x);
        children = 0;
        int b = hmax(x), len = -1;
        if (solved_x(x)) len = 0;
        else for (;; b++) if (dfs(x, 0, b, 3)) { len = b; break; }
        if (len != d) bad++;
        if (n == cap) { cap *= 2; v = realloc(v, cap * sizeof *v); }
        v[n++] = children; sum += children;
    }
    fclose(f);
    qsort(v, n, sizeof *v, cmpu);
    printf("states %zu (optimality mismatches %d): children mean %.0f p50 %llu p90 %llu p99 %llu max %llu\n",
           n, bad, (double)sum / n, (unsigned long long)v[n / 2], (unsigned long long)v[n * 9 / 10],
           (unsigned long long)v[n * 99 / 100], (unsigned long long)v[n - 1]);
}

static double mean_h(int *ids, int m, uint32_t stride)
{
    double sum = 0; uint32_t cnt = 0;
    for (uint32_t r = 0; r < STATES; r += stride, cnt++) {
        int h = 0;
        for (int i = 0; i < m; i++) { int v = tab[ids[i]][sub_index(ids[i], XS[r])]; if (v > h) h = v; }
        sum += h;
    }
    return sum / cnt;
}

static void load_all(const char *distpath)
{
    dist = load_dist(distpath);
    XS = malloc((size_t)STATES * 8);
    for (uint32_t r = 0; r < STATES; r++) { state_t s; unrank_state(r, &s); memset(XS[r], 0, 8); cube_to_x(&s, XS[r]); }
    build_qt(qt);
    tab = malloc(40 * (size_t)PDB_ENTRIES);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s sweep [dist] | eval MASKS [d11.txt] [dist]\n", argv[0]); return 2; }
    if (!strcmp(argv[1], "sweep")) {
        load_all(argc > 2 ? argv[2] : "build/dist.bin");
        for (int m = 0; m < 128; m++) if (__builtin_popcount(m) == 4) submask[nsub++] = m;
        for (int s = 0; s < nsub; s++) build_sub(s);
        printf("single 4-cubie tables (cubies 1-based), mean h over every 7th state:\n");
        for (int s = 0; s < nsub; s++) {
            int id[1] = {s}; printf("  %02x {", submask[s]);
            for (int c = 0; c < 7; c++) if (submask[s] >> c & 1) printf("%d", c + 1);
            printf("}  %.3f\n", mean_h(id, 1, 7));
        }
        /* all pairs */
        static double pm[40][40]; double best = 0; int ba = 0, bb = 1;
        for (int a = 0; a < nsub; a++) for (int b = a + 1; b < nsub; b++) { int id[2] = {a, b}; pm[a][b] = mean_h(id, 2, 7); if (pm[a][b] > best) { best = pm[a][b]; ba = a; bb = b; } }
        printf("best pair: %02x + %02x  mean h %.3f\n", submask[ba], submask[bb], best);
        double bt = 0; int bc = -1;
        for (int c = 0; c < nsub; c++) if (c != ba && c != bb) { int id[3] = {ba, bb, c}; double v = mean_h(id, 3, 7); if (v > bt) { bt = v; bc = c; } }
        printf("best third table: %02x -> mean h %.3f (3 tables = %u nibble bytes)\n", submask[bc], bt, 3 * PDB_BYTES);
        printf("mean true distance: 8.756\n");
        return 0;
    }
    if (!strcmp(argv[1], "eval") && argc > 2) {
        load_all(argc > 4 ? argv[4] : "build/dist.bin");
        char *p = argv[2], *end;
        while (*p) {
            int m = (int)strtol(p, &end, 16);
            if (__builtin_popcount(m) != 4) { fprintf(stderr, "mask %x must have 4 bits\n", m); return 2; }
            submask[nsub] = m; build_sub(nsub); use[nuse++] = nsub++;
            p = *end == ',' ? end + 1 : end;
            if (end == p - 0 && !*end) break;
        }
        printf("tables:"); for (int i = 0; i < nuse; i++) printf(" %02x", submask[use[i]]); printf("\n");
        printf("mean h (every 7th state): %.3f\n", mean_h(use, nuse, 7));
        eval_nodes(argc > 3 ? argv[3] : "build/d11.txt");
        return 0;
    }
    fprintf(stderr, "bad arguments\n"); return 2;
}
