/* solver_core.h - freestanding IDA* core (no libc, no heap, no recursion). */
#ifndef SOLVER_CORE_H
#define SOLVER_CORE_H
#include <stdint.h>

#define SOLVE_E_INPUT (-1)   /* malformed / invalid cube string            */
#define SOLVE_E_BOUND (-2)   /* internal error: bound exceeded the diameter */

/* in: 14 chars "PPPPPPPOOOOOOO" (not NUL-terminated, exactly 14 read).
 * path: >= 12 entries; each is a move id 0..8 = face*3+turn
 *       (0 R, 1 R2, 2 R', 3 B, 4 B2, 5 B', 6 D, 7 D2, 8 D').
 * Returns the optimal length (>= 0) or a negative SOLVE_E_* code. */
int solve(const char in[14], uint8_t path[12]);

/* Production heuristic accessors (same code the search uses), for gates. */
uint8_t solver_h_a(const uint8_t x[7]);
uint8_t solver_h_b(const uint8_t x[7]);
uint8_t solver_h(const uint8_t x[7]);
const uint8_t *solver_pdb(int which);   /* 0 = A, 1 = B (nibble packed) */
const uint8_t *solver_qt(void);         /* 96-byte quarter-turn table   */
#endif
