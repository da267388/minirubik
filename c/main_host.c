/* main_host.c - CLI around solver_core. Verifies the path with the model. */
#include <stdio.h>
#include <string.h>
#include "solver_core.h"
#include "cube_model.h"
#include "opcount.h"

int main(int argc, char **argv)
{
    state_t s;
    uint8_t path[12];
    if (argc != 2 || strlen(argv[1]) != 14 || !parse_state_model(argv[1], &s)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n", argc > 0 ? argv[0] : "solver");
        return 2;
    }
    int n = solve(argv[1], path);
    if (n < 0) { fprintf(stderr, "solver error %d\n", n); return 1; }
    for (int i = 0; i < n; i++) {
        printf("%s%s", i ? " " : "", move_names[path[i]]);
        s = apply_move(s, path[i]);
    }
    putchar('\n');
    if (!is_solved(&s)) { fprintf(stderr, "path does not solve the cube\n"); return 1; }
#ifdef OPCOUNT
    fprintf(stderr, "len=%d iters=%llu children=%llu descended=%llu hlook=%llu idx=%llu\n",
            n, (unsigned long long)opc.iters, (unsigned long long)opc.children,
            (unsigned long long)opc.descended, (unsigned long long)opc.hlook,
            (unsigned long long)opc.idx);
#endif
    return fflush(stdout) != 0;
}
