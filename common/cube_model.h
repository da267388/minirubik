/* cube_model.h - host-side model of the 2x2x2 cube (fixed corner), HTM.
 * source/twist are copied verbatim from solver.c.  Used by host tools and
 * tests ONLY; the freestanding solver core never includes this file.
 *
 * Two state views:
 *   state_t : position-indexed  (p[i] = cubie at position i, o[i] = its twist)
 *   x[7]    : cubie-indexed     (x[c] = position<<2 | twist), used by the solver
 */
#ifndef CUBE_MODEL_H
#define CUBE_MODEL_H
#include <stdint.h>
#include <string.h>

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

enum { CUBIES = 7, PERMUTATIONS = 5040, ORIENTATIONS = 729,
       STATES = 3674160, MOVES = 9 };

typedef struct { uint8_t p[CUBIES], o[CUBIES]; } state_t;

UNUSED static const char *const move_names[MOVES] = {
    "R", "R2", "R'", "B", "B2", "B'", "D", "D2", "D'"};
UNUSED static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6}, {0, 1, 2, 4, 5, 6, 3}, {0, 2, 5, 3, 1, 4, 6}};
UNUSED static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0}, {0, 0, 0, 1, 2, 1, 2}, {0, 0, 0, 0, 0, 0, 0}};

static inline state_t solved_state(void)
{
    state_t s;
    for (int i = 0; i < CUBIES; i++) s.p[i] = (uint8_t)i, s.o[i] = 0;
    return s;
}

static inline state_t quarter(const state_t *s, int face)
{
    state_t t;
    for (int i = 0; i < CUBIES; i++) {
        t.p[i] = s->p[source[face][i]];
        t.o[i] = (uint8_t)((s->o[source[face][i]] + twist[face][i]) % 3);
    }
    return t;
}

/* move = face*3 + turn; turn 0,1,2 = 1,2,3 quarter turns (R, R2, R') */
static inline state_t apply_move(state_t s, int move)
{
    for (int k = 0; k < move % 3 + 1; k++) s = quarter(&s, move / 3);
    return s;
}

static inline int is_solved(const state_t *s)
{
    for (int i = 0; i < CUBIES; i++) if (s->p[i] != i || s->o[i]) return 0;
    return 1;
}

static inline uint32_t rank_state(const state_t *s)
{
    uint32_t p = 0, o = 0;
    for (int i = 0; i < CUBIES; i++) {
        uint32_t c = 0;
        for (int j = i + 1; j < CUBIES; j++) c += s->p[j] < s->p[i];
        p = p * (CUBIES - i) + c;
    }
    for (int i = 0; i < 6; i++) o = o * 3 + s->o[i];
    return p * ORIENTATIONS + o;
}

static inline void unrank_state(uint32_t r, state_t *s)
{
    uint32_t p = r / ORIENTATIONS, o = r % ORIENTATIONS, sum = 0, f = 720;
    uint8_t avail[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    for (int i = 5; i >= 0; i--) { s->o[i] = (uint8_t)(o % 3); o /= 3; sum += s->o[i]; }
    s->o[6] = (uint8_t)((3 - sum % 3) % 3);
    for (int i = 0; i < CUBIES; i++) {
        uint32_t q = p / f; p %= f;
        s->p[i] = avail[q];
        for (int k = (int)q; k < CUBIES - i - 1; k++) avail[k] = avail[k + 1];
        if (i < CUBIES - 1) f /= (uint32_t)(CUBIES - 1 - i);
    }
}

static inline void format_state(const state_t *s, char out[15])
{
    for (int i = 0; i < CUBIES; i++) {
        out[i] = (char)('1' + s->p[i]);
        out[7 + i] = (char)('1' + s->o[i]);
    }
    out[14] = 0;
}

/* Strict parse: exactly 14 chars, valid permutation, twist sum % 3 == 0. */
static inline int parse_state_model(const char *in, state_t *s)
{
    if (strlen(in) != 14) return 0;
    unsigned seen = 0, sum = 0;
    for (int i = 0; i < CUBIES; i++) {
        int c = in[i] - '1', o = in[7 + i] - '1';
        if (c < 0 || c > 6 || o < 0 || o > 2 || (seen >> c & 1)) return 0;
        seen |= 1u << c; sum += (unsigned)o;
        s->p[i] = (uint8_t)c; s->o[i] = (uint8_t)o;
    }
    return sum % 3 == 0;
}

/* cubie-indexed view: x[c] = position<<2 | twist */
static inline void cube_to_x(const state_t *s, uint8_t x[7])
{
    for (int i = 0; i < CUBIES; i++) x[s->p[i]] = (uint8_t)(i << 2 | s->o[i]);
}

#endif
