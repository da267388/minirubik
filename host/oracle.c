/* oracle.c - exact BFS distances. Writes DIR/dist.bin (3,674,160 bytes,
 * index = rank), DIR/d11.txt (every distance-11 state) and
 * DIR/samples_d8_10.txt (500 random states each at distance 8, 9, 10).
 * Self-checks against the published distribution before writing anything. */
#include <stdio.h>
#include <stdlib.h>
#include "cube_model.h"

static uint64_t rng = 0x9E3779B97F4A7C15ull;      /* fixed seed */
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[512], buf[16];
    /* move inversion: four quarter turns of a face is the identity */
    for (int f = 0; f < 3; f++) {
        state_t s = solved_state();
        for (int k = 0; k < 4; k++) s = quarter(&s, f);
        state_t id = solved_state();
        if (memcmp(&s, &id, sizeof s)) { fprintf(stderr, "face %d: order != 4\n", f); return 1; }
    }
    uint8_t *dist = malloc(STATES);
    uint32_t *queue = malloc(STATES * sizeof *queue), head = 0, tail = 0;
    memset(dist, 0xFF, STATES);
    dist[0] = 0; queue[tail++] = 0;
    while (head < tail) {
        uint32_t r = queue[head++];
        state_t s; unrank_state(r, &s);
        for (int f = 0; f < 3; f++) {
            state_t t = s;
            for (int k = 0; k < 3; k++) {
                t = quarter(&t, f);
                uint32_t q = rank_state(&t);
                if (dist[q] == 0xFF) { dist[q] = (uint8_t)(dist[r] + 1); queue[tail++] = q; }
            }
        }
    }
    static const uint32_t expect[12] = {1, 9, 54, 321, 1847, 9992, 50136,
        227536, 870072, 1887748, 623800, 2644};
    uint32_t hist[256] = {0};
    for (uint32_t r = 0; r < STATES; r++) hist[dist[r]]++;
    int ok = tail == STATES && hist[0xFF] == 0;
    for (int d = 0; d < 12; d++) if (hist[d] != expect[d]) ok = 0;
    printf("oracle: reached %u states, diameter check %s\n", tail, ok ? "PASS" : "FAIL");
    if (!ok) return 1;

    snprintf(path, sizeof path, "%s/dist.bin", dir);
    FILE *fp = fopen(path, "wb"); if (!fp) { perror(path); return 1; }
    fwrite(dist, 1, STATES, fp); fclose(fp);

    snprintf(path, sizeof path, "%s/d11.txt", dir);
    fp = fopen(path, "w"); if (!fp) { perror(path); return 1; }
    for (uint32_t r = 0; r < STATES; r++)
        if (dist[r] == 11) { state_t s; unrank_state(r, &s); format_state(&s, buf); fprintf(fp, "%s 11\n", buf); }
    fclose(fp);

    snprintf(path, sizeof path, "%s/samples_d8_10.txt", dir);
    fp = fopen(path, "w"); if (!fp) { perror(path); return 1; }
    for (int d = 8; d <= 10; d++) {
        uint32_t *pool = malloc(hist[d] * sizeof *pool), n = 0;
        for (uint32_t r = 0; r < STATES; r++) if (dist[r] == d) pool[n++] = r;
        for (uint32_t k = 0; k < 500; k++) {
            uint32_t j = k + rnd() % (n - k), t = pool[k]; pool[k] = pool[j]; pool[j] = t;
            state_t s; unrank_state(pool[k], &s); format_state(&s, buf);
            fprintf(fp, "%s %d\n", buf, d);
        }
        free(pool);
    }
    fclose(fp);
    printf("wrote dist.bin, d11.txt (%u), samples_d8_10.txt (1500)\n", hist[11]);
    return 0;
}
