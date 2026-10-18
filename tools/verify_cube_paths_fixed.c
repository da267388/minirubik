/*
 * verify_cube_paths.c - Verify 2x2x2 cube solutions using the EXACT QT table
 * from the RV32I solver (NOT a guessed cube rotation convention).
 *
 * Case-file format, one case per line:
 *   25416373331111|R' B R' B R' B2 D2 B' R' B2
 *   21345671111111|B' R' D2 R' B R B' R D2 B R'
 *   14325671111111|D R' B R D' B2 D' R' B' R B2
 *
 * Commands:
 *   gcc -std=c11 -O2 -Wall -Wextra -pedantic verify_cube_paths.c -o verify_cube_paths
 *   ./verify_cube_paths cases.txt asm/data/consts.inc
 *   ./verify_cube_paths cases.txt asm/data/consts.inc --trace
 *   ./verify_cube_paths cases.txt qt.bin --face-order RBD --trace
 *
 * QT input:
 *  1) An assembler source containing a QT: label followed by 96 data bytes
 *     in .byte/.2byte/.word directives, or an .incbin directive; or
 *  2) A raw binary QT with exactly 96 bytes (3 faces * 32 values).
 *
 * Encoded cubies: state[c] = (initial_position_index << 2) | twist;
 * Goal state:      {0,4,8,12,16,20,24}
 * Face indices:    selectable --face-order RBD or RDB (default RBD).
 * R2/D2/B2 apply a quarter turn twice; prime applies it three times.
 *
 * This program DOES NOT rely on PDB/IDA* and independently replays the output.
 */

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QT_BYTES 96
#define CUBIES 7
#define LINE_CAP 8192

static const uint8_t goal[CUBIES] = {0, 4, 8, 12, 16, 20, 24};
static uint8_t qt[QT_BYTES];
static const char *face_order = "RBD";

static int face_index(char ch) {
    const char *p = strchr(face_order, ch);
    return p ? (int)(p - face_order) : -1;
}

static const char *skip_space(const char *p) {
    while (*p && isspace((unsigned char)*p)) ++p;
    return p;
}

static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static char *ltrim(char *s) {
    return (char *)skip_space(s);
}

static void strip_comment(char *line) {
    char *p = strchr(line, '#');
    if (p) *p = '\0';
    p = strstr(line, "//");
    if (p) *p = '\0';
    /* On RISC-V, ';' may separate statements; data parser uses only first. */
    p = strchr(line, ';');
    if (p) *p = '\0';
}

static int parse_int_bytes(char *args, uint8_t *out, size_t *used,
                           size_t count, size_t item_bytes) {
    char *p = args;
    int found = 0;
    for (;;) {
        p = ltrim(p);
        while (*p == ',') p = ltrim(p + 1);
        if (!*p) break;
        errno = 0;
        char *end;
        long v = strtol(p, &end, 0);
        if (end == p || errno == ERANGE) return -1;
        if (*used + item_bytes > count) return -1;
        unsigned long u = (unsigned long)v;
        for (size_t b = 0; b < item_bytes; ++b)
            out[(*used)++] = (uint8_t)(u >> (b * 8));
        found = 1;
        p = end;
        p = ltrim(p);
        if (*p == ',') ++p;
        else if (*p && *p != '\r' && *p != '\n') return -1;
    }
    return found ? 0 : -1;
}

static int parse_incbin(char *arg, const char *asm_path,
                        uint8_t *out, size_t *used) {
    arg = ltrim(arg);
    if (*arg != '"') return -1;
    ++arg;
    char *end = strchr(arg, '"');
    if (!end) return -1;
    *end = '\0';

    FILE *fp = fopen(arg, "rb");
    if (!fp) {
        /* Resolve filenames relative to .inc file. */
        char path[LINE_CAP];
        const char *slash = strrchr(asm_path, '/');
        if (!slash) return -1;
        size_t prefix = (size_t)(slash - asm_path + 1);
        if (prefix + strlen(arg) + 1 > sizeof(path)) return -1;
        memcpy(path, asm_path, prefix);
        strcpy(path + prefix, arg);
        fp = fopen(path, "rb");
    }
    if (!fp) return -1;
    size_t rem = QT_BYTES - *used;
    size_t n = fread(out + *used, 1, rem, fp);
    fclose(fp);
    *used += n;
    return n == rem ? 0 : -1;
}

static int read_qt_assembly(const char *filename, uint8_t out[QT_BYTES]) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open QT assembly file: %s\n", filename);
        return -1;
    }

    char line[LINE_CAP];
    size_t used = 0;
    int in_qt = 0;
    unsigned lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        ++lineno;
        strip_comment(line);
        char *p = ltrim(line);
        rtrim(p);
        if (!*p) continue;

        if (!in_qt) {
            /* Accept 'QT:' with optional instructions on the same line. */
            if (strncmp(p, "QT:", 3) != 0) continue;
            in_qt = 1;
            p = ltrim(p + 3);
            if (!*p) continue;
        }

        if (used == QT_BYTES) break;
        if (*p == '.') {
            size_t bytes = 0;
            if (strncmp(p, ".byte", 5) == 0 && isspace((unsigned char)p[5])) {
                bytes = 1; p += 5;
            } else if (strncmp(p, ".2byte", 6) == 0 && isspace((unsigned char)p[6])) {
                bytes = 2; p += 6;
            } else if (strncmp(p, ".half", 5) == 0 && isspace((unsigned char)p[5])) {
                bytes = 2; p += 5;
            } else if (strncmp(p, ".word", 5) == 0 && isspace((unsigned char)p[5])) {
                bytes = 4; p += 5;
            } else if (strncmp(p, ".4byte", 6) == 0 && isspace((unsigned char)p[6])) {
                bytes = 4; p += 6;
            } else if (strncmp(p, ".incbin", 7) == 0 && isspace((unsigned char)p[7])) {
                if (parse_incbin(p + 7, filename, out, &used) != 0) goto bad;
                continue;
            } else if (strncmp(p, ".align", 6) == 0 ||
                       strncmp(p, ".balign", 7) == 0 ||
                       strncmp(p, ".p2align", 8) == 0) {
                continue;
            } else {
                fprintf(stderr, "Unsupported directive in QT at %s:%u: %s\n",
                        filename, lineno, p);
                goto bad;
            }
            if (parse_int_bytes(p, out, &used, QT_BYTES, bytes) != 0) {
                fprintf(stderr, "Invalid QT data in %s:%u: %s\n",
                        filename, lineno, line);
                goto bad;
            }
        } else {
            fprintf(stderr, "Unexpected label/text inside QT at %s:%u: %s\n",
                    filename, lineno, p);
            goto bad;
        }
    }
    fclose(fp);
    if (!in_qt) {
        fprintf(stderr, "QT: label not found in %s\n", filename);
        return -1;
    }
    if (used != QT_BYTES) {
        fprintf(stderr, "QT has %zu bytes, expected exactly %d.\n", used, QT_BYTES);
        return -1;
    }
    return 0;

bad:
    fclose(fp);
    return -1;
}

static int read_qt(const char *filename) {
    size_t n = strlen(filename);
    if (n >= 4 && strcmp(filename + n - 4, ".bin") == 0) {
        FILE *fp = fopen(filename, "rb");
        if (!fp) { perror(filename); return -1; }
        size_t used = fread(qt, 1, QT_BYTES, fp);
        int extra = fgetc(fp);
        fclose(fp);
        if (used != QT_BYTES || extra != EOF) {
            fprintf(stderr, "QT binary must be exactly 96 bytes: %s\n", filename);
            return -1;
        }
    } else if (read_qt_assembly(filename, qt) != 0) return -1;

    /* Only twists 0,1,2 are legal. The unused 4th entry of each
     * position may legitimately hold a sentinel such as 0xFF. */
    for (unsigned f = 0; f < 3; ++f) {
        for (unsigned x = 0; x < 28; ++x) {
            if ((x & 3u) == 3u) continue;
            unsigned out = qt[f * 32 + x];
            if (out >= 28 || (out & 3u) == 3u) {
                fprintf(stderr, "Invalid QT[%u][%u] = %u (not a valid encoded cubie)\n", f, x, out);
                return -1;
            }
        }
    }
    return 0;
}

/* Parsing faithfully follows the user's C/ASM:
 *   c = first7[i] - '1'
 *   o = last7[i]  - '1'
 *   state[c] = (i << 2) | o
 */
static int parse_state(const char *s, uint8_t st[CUBIES],
                       char *error, size_t error_cap) {
    if (strlen(s) != 14) {
        snprintf(error, error_cap, "state must have exactly 14 digits");
        return -1;
    }
    unsigned seen = 0, sum = 0;
    for (unsigned i = 0; i < CUBIES; ++i) {
        if (s[i] < '1' || s[i] > '7' || s[i + 7] < '1' || s[i + 7] > '3') {
            snprintf(error, error_cap, "invalid state digit at position %u", i);
            return -1;
        }
        unsigned c = (unsigned)(s[i] - '1');
        unsigned o = (unsigned)(s[i + 7] - '1');
        if (seen & (1u << c)) {
            snprintf(error, error_cap, "repeated cubie ID %u", c + 1);
            return -1;
        }
        seen |= 1u << c;
        sum += o;
        st[c] = (uint8_t)((i << 2) | o);
    }
    if (sum % 3 != 0) {
        snprintf(error, error_cap, "orientation sum %% 3 != 0");
        return -1;
    }
    return 0;
}

static int do_move(uint8_t st[CUBIES], int face, int turns) {
    for (int t = 0; t < turns; ++t) {
        for (unsigned c = 0; c < CUBIES; ++c) {
            if (st[c] >= 32) return -1;
            st[c] = qt[face * 32 + st[c]];
            if (st[c] >= 28 || (st[c] & 3u) == 3u) return -1;
        }
    }
    return 0;
}

static void print_state(const uint8_t st[CUBIES]) {
    for (unsigned c = 0; c < CUBIES; ++c)
        printf("%s%02X", c ? " " : "", st[c]);
}

static int same_state(const uint8_t a[CUBIES], const uint8_t b[CUBIES]) {
    return memcmp(a, b, CUBIES) == 0;
}

static int run_case(unsigned line_no, char *line, int trace) {
    char *sep = strchr(line, '|');
    if (!sep) {
        printf("Line %u: INVALID: missing '|'\n", line_no);
        return -1;
    }
    *sep = '\0';
    char *initial = ltrim(line);
    rtrim(initial);
    char *moves = ltrim(sep + 1);
    rtrim(moves);

    uint8_t st[CUBIES];
    char error[128];
    if (parse_state(initial, st, error, sizeof(error)) != 0) {
        printf("Line %u: INVALID INPUT (%s): %s\n", line_no, initial, error);
        return -1;
    }

    unsigned steps = 0;
    const char *p = moves;
    printf("Line %u: %s | %s\n", line_no, initial, moves);
    if (trace) {
        printf("  initial  ");
        print_state(st);
        putchar('\n');
    }

    while (*p) {
        p = skip_space(p);
        if (!*p) break;
        const char *start = p;
        int face = face_index(*p++);
        if (face < 0) {
            printf("  INVALID MOVE at character %zu: '%c'\n",
                   (size_t)(start - moves + 1), *start);
            return -1;
        }
        int turns = 1;
        if (*p == '2') { turns = 2; ++p; }
        else if (*p == '\'') { turns = 3; ++p; }
        if (*p && !isspace((unsigned char)*p) &&
            *p != 'R' && *p != 'D' && *p != 'B') {
            printf("  INVALID MOVE suffix near character %zu\n",
                   (size_t)(p - moves + 1));
            return -1;
        }
        if (do_move(st, face, turns) != 0) {
            printf("  INVALID QT transition at step %u\n", steps + 1);
            return -1;
        }
        ++steps;
        if (trace) {
            printf("  step %-2u %-2.*s : ", steps, (int)(p - start), start);
            print_state(st);
            putchar('\n');
        }
    }

    const int solved = same_state(st, goal);
    printf("  steps: %-2u  %s\n", steps, solved ? "PASS (solved)" : "FAIL (not solved)");
    if (!solved || trace) {
        printf("  final:    "); print_state(st); putchar('\n');
        printf("  expected: "); print_state(goal); putchar('\n');
    }
    return solved ? 0 : 1;
}

int main(int argc, char **argv) {
    int trace = 0;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <cases.txt> <QT-assembly.inc|qt.bin> [--face-order RBD|RDB] [--trace]\n", argv[0]);
        return 2;
    }
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "--face-order") == 0 && i + 1 < argc) {
            face_order = argv[++i];
            if (strlen(face_order) != 3 || strchr(face_order, 'R') == NULL ||
                strchr(face_order, 'B') == NULL || strchr(face_order, 'D') == NULL ||
                face_order[0] == face_order[1] || face_order[0] == face_order[2] ||
                face_order[1] == face_order[2]) {
                fprintf(stderr, "Face order must be a permutation of RBD (e.g. RBD or RDB)\n");
                return 2;
            }
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 2;
        }
    }
    printf("QT face order: %c=%d, %c=%d, %c=%d\n", face_order[0], 0,
           face_order[1], 1, face_order[2], 2);
    if (read_qt(argv[2]) != 0) return 2;
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror(argv[1]); return 2; }

    unsigned line_no = 0, tested = 0, passed = 0, failed = 0, invalid = 0;
    char line[LINE_CAP];
    while (fgets(line, sizeof(line), fp)) {
        ++line_no;
        if (!strchr(line, '\n') && !feof(fp)) {
            fprintf(stderr, "Input line %u is too long\n", line_no);
            fclose(fp);
            return 2;
        }
        char *p = ltrim(line);
        rtrim(p);
        if (!*p || *p == '#') continue;
        int result = run_case(line_no, p, trace);
        ++tested;
        if (result == 0) ++passed;
        else if (result == 1) ++failed;
        else ++invalid;
    }
    if (ferror(fp)) { perror("reading cases"); fclose(fp); return 2; }
    fclose(fp);

    printf("\nSUMMARY: %u cases, %u PASS, %u FAIL, %u INVALID\n",
           tested, passed, failed, invalid);
    return (failed || invalid || tested == 0) ? 1 : 0;
}
