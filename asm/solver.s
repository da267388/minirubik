.include "asm/data/consts.inc"

# characters for output
.equ O_R, 82
.equ O_D, 68
.equ O_B, 66
.equ O_2, 50
.equ O_I, 39 # '\''

.data

state: .string "14325671111111"

move_face: .byte 0, 0, 0, 1, 1, 1, 2, 2, 2
move_turn: .byte 0, 1, 2, 0, 1, 2, 0, 1, 2

.bss
st:
    .zero  104

last_face:
    .zero  12

next_move:
    .zero  12

taken:
    .zero  12

.text
.global _start;
# ============================================================
# Register allocation in search
#
# s0 = st base
# s1 = QT base
# s2 = PDB_A base
# s3 = PDB_B base
#
# a1 = last_face base
# a2 = next_move base
# a3 = move_face base
# a4 = move_turn base
# a5 = taken base
#
# t0 = bound
# t1 = maximum bound + 1 (12)
# t2 = d
# t3 = m
# t4 = f
# t5 = scratch
# t6 = g
#
# a7 = saved ha, during lazy heuristic
#
# h_a / h_b:
#   input a0 = state pointer
#   output a6 = heuristic
#   preserve a0, t0-t4, t6, a1-a5, a7, s0-s3
#   clobber t5, s4-s11, a6
#
# IMPORTANT: custom internal calling convention.
# ============================================================

_start:
    # Initialize all long-lived bases BEFORE heuristics.
    la s0, st
    la s1, QT
    la s2, PDB_A
    la s3, PDB_B

    la a1, last_face
    la a2, next_move
    la a3, move_face
    la a4, move_turn
    la a5, taken

    # --------------------------------------------------------
    # parse input
    #
    # input:
    #   first 7 chars  = cubie IDs '1'..'7'
    #   second 7 chars = orientations '1'..'3'
    #
    # st[0][c] = (i << 2) | orientation
    # --------------------------------------------------------

    la t0, state          # position characters
    addi t1, t0, 7       # orientation characters

    li t2, 0             # i
    li t3, 0             # seen mask
    li t4, 0             # orientation sum

parse_loop:
    lbu t5, 0(t0)        # position character
    lbu t6, 0(t1)        # orientation character

    addi t5, t5, -49     # c = char - '1'
    li a6, 7
    bgeu t5, a6, SOLVE_E_INPUT

    addi t6, t6, -49     # o = char - '1'
    li a6, 3
    bgeu t6, a6, SOLVE_E_INPUT

    # Duplicate cubie check
    li a6, 1
    sll a6, a6, t5

    and a0, a6, t3
    bnez a0, SOLVE_E_INPUT

    or t3, t3, a6

    # sum += o
    add t4, t4, t6

    # encoded = (i << 2) | o
    slli a6, t2, 2
    or a6, a6, t6

    # st[0][c] = encoded
    # uint8_t => offset is c, NOT c*8
    add a0, s0, t5
    sb a6, 0(a0)

    # next input character
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, 1

    li a6, 7
    bltu t2, a6, parse_loop

    # sum % 3
    li a6, 3

parse_mod:
    bltu t4, a6, parse_mod_done
    addi t4, t4, -3
    j parse_mod

parse_mod_done:
    bnez t4, SOLVE_E_INPUT

    # --------------------------------------------------------
    # Initial heuristic h0 = max(hA, hB)
    # --------------------------------------------------------

    mv a0, s0
    call h_a

    mv t0, a6            # t0 = ha

    # a0 still points to st[0]
    call h_b

    # t0 = max(ha, hb)
    bgeu t0, a6, initial_h_done
    mv t0, a6

initial_h_done:
    # Initial state is already solved
    beqz t0, solved_initial

    # Maximum bound = 11
    li t1, 12

# ============================================================
# IDA* outer loop
#
# for (bound = h0; bound <= 11; bound++)
# ============================================================

boundloop:
    bgeu t0, t1, SOLVE_E_BOUND

    li t2, 0             # d = 0

    # last_face[0] = 3 (no previous face)
    li t5, 3
    sb t5, 0(a1)

    # next_move[0] = 0
    sb zero, 0(a2)

# ============================================================
# Explicit DFS
# ============================================================

dfsloop:
    # m = next_move[d]
    add t5, a2, t2
    lbu t3, 0(t5)

    # Check m before accessing move_face[m]
    li t5, 9
    bgeu t3, t5, exhausted

    # face = move_face[m]
    add t5, a3, t3
    lbu t4, 0(t5)

    # previous face = last_face[d]
    add t5, a1, t2
    lbu t5, 0(t5)

    # if same face, m += 3
    bne t4, t5, check_exhausted
    addi t3, t3, 3

check_exhausted:
    li t5, 9
    bgeu t3, t5, exhausted

    j move

exhausted:
    # All moves at this depth have been tried.
    # If root is exhausted, increase IDA* bound.
    beqz t2, bound_next

    # Backtrack one level
    addi t2, t2, -1
    j dfsloop

bound_next:
    addi t0, t0, 1
    j boundloop

# ============================================================
# Generate child
# ============================================================

move:
    # next_move[d] = m + 1
    add t5, a2, t2
    addi t4, t3, 1
    sb t4, 0(t5)

    # f = move_face[m]
    add t5, a3, t3
    lbu t4, 0(t5)

    # dst = &st[d+1][0]
    addi t5, t2, 1
    slli t5, t5, 3
    add t5, s0, t5

    # Default src = dst
    mv t6, t5

    # move_turn[m]
    add a6, a4, t3
    lbu a6, 0(a6)

    # If move_turn[m] != 0, reuse previous child.
    bnez a6, turn_begin

    # Otherwise src = &st[d][0]
    slli t6, t2, 3
    add t6, s0, t6

turn_begin:
    # QT + face*32
    slli s4, t4, 5
    add s4, s1, s4

    # turn(dst, src, face)
    # Seven independent byte transformations.

    lbu a6, 0(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 0(t5)

    lbu a6, 1(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 1(t5)

    lbu a6, 2(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 2(t5)

    lbu a6, 3(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 3(t5)

    lbu a6, 4(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 4(t5)

    lbu a6, 5(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 5(t5)

    lbu a6, 6(t6)
    add a6, s4, a6
    lbu a6, 0(a6)
    sb a6, 6(t5)

    # --------------------------------------------------------
    # Lazy PDB evaluation
    #
    # g = d + 1
    # --------------------------------------------------------

    addi t6, t2, 1

    # h_a(child)
    mv a0, t5
    call h_a

    # a7 = ha
    mv a7, a6

    # if (g + ha > bound) continue
    add t5, t6, a7
    bltu t0, t5, dfsloop

    # h_b(child)
    # a0 remains child pointer
    call h_b

    # if (g + hb > bound) continue
    add t5, t6, a6
    bltu t0, t5, dfsloop

    # if ((ha | hb) == 0) found
    or t5, a7, a6
    beqz t5, found

    # --------------------------------------------------------
    # Descend
    # --------------------------------------------------------

    # taken[d] = m
    add t5, a5, t2
    sb t3, 0(t5)

    # last_face[d+1] = f
    add t5, a1, t2
    sb t4, 1(t5)

    # next_move[d+1] = 0
    add t5, a2, t2
    sb zero, 1(t5)

    # d++
    addi t2, t2, 1
    j dfsloop

# ============================================================
# Found solution
#
# taken[0..d-1] are previous moves.
# t3 is the final move.
#
# Print directly; do not create path[].
# ============================================================

found:
    # Save final move in t3.
    # t2 becomes the output index.
    li t2, 0

found_loop:
    # while (i < d)
    # d is recovered from the pointer to current state:
    # use t6 = d+1, so d = t6-1
    addi t4, t6, -1
    bgeu t2, t4, found_last

    add t5, a5, t2
    lbu a6, 0(t5)

    # print_move preserves t2,t3,t6,a5
    call print_move

    addi t2, t2, 1
    j found_loop

found_last:
    mv a6, t3
    call print_move

    li a0, 10
    li a7, 11
    ecall

    j end

solved_initial:
    # Empty solution
    li a0, 10
    li a7, 11
    ecall
    j end

# ============================================================
# Exit
# ============================================================

end:
    li a0, 0
    li a7, 93
    ecall

SOLVE_E_INPUT:
    li a0, 69            # E
    li a7, 11
    ecall

    li a0, -1
    li a7, 93
    ecall

SOLVE_E_BOUND:
    li a0, 69            # E
    li a7, 11
    ecall

    li a0, -2
    li a7, 93
    ecall

# ============================================================
# h_a
#
# input:  a0 = state pointer
# output: a6 = heuristic A
#
# s2 = PDB_A base
# ============================================================

h_a:
    lbu s4, A0(a0)
    lbu s5, A1(a0)
    lbu s6, A2(a0)
    lbu s7, A3(a0)

    # orientation index
    andi s8, s4, 3

    slli s9, s8, 1
    add s8, s8, s9
    andi s9, s5, 3
    add s8, s8, s9

    slli s9, s8, 1
    add s8, s8, s9
    andi s9, s6, 3
    add s8, s8, s9

    slli s9, s8, 1
    add s8, s8, s9
    andi s9, s7, 3
    add s8, s8, s9

    # positions
    srli s4, s4, 2
    srli s5, s5, 2
    srli s6, s6, 2
    srli s7, s7, 2

    # b1
    sltu s9, s4, s5
    sub s9, s5, s9

    # c1
    sltu s11, s4, s6
    sub s10, s6, s11

    sltu s11, s5, s6
    sub s10, s10, s11

    # d1
    sltu a6, s4, s7
    sub s11, s7, a6

    sltu a6, s5, s7
    sub s11, s11, a6

    sltu a6, s6, s7
    sub s11, s11, a6

    # permutation rank
    slli a6, s4, 2
    add s9, s9, a6

    slli a6, s4, 1
    add s9, s9, a6

    slli a6, s9, 2
    add s9, s9, a6

    add s9, s9, s10
    slli s9, s9, 2
    add s9, s9, s11

    # index = permutation * 81 + orientation
    slli a6, s9, 6
    add s8, s8, a6

    slli a6, s9, 4
    add s8, s8, a6

    add s8, s8, s9

    # nibble-packed PDB lookup
    srli s9, s8, 1
    add s9, s2, s9
    lbu s9, 0(s9)

    andi s10, s8, 1
    slli s10, s10, 2
    srl s9, s9, s10

    andi a6, s9, 15
    ret

# ============================================================
# h_b
#
# input:  a0 = state pointer
# output: a6 = heuristic B
#
# s3 = PDB_B base
# ============================================================

h_b:
    lbu s4, B0(a0)
    lbu s5, B1(a0)
    lbu s6, B2(a0)
    lbu s7, B3(a0)

    # orientation index
    andi s8, s4, 3

    slli s9, s8, 1
    add s8, s8, s9
    andi s9, s5, 3
    add s8, s8, s9

    slli s9, s8, 1
    add s8, s8, s9
    andi s9, s6, 3
    add s8, s8, s9

    slli s9, s8, 1
    add s8, s8, s9
    andi s9, s7, 3
    add s8, s8, s9

    # positions
    srli s4, s4, 2
    srli s5, s5, 2
    srli s6, s6, 2
    srli s7, s7, 2

    # b1
    sltu s9, s4, s5
    sub s9, s5, s9

    # c1
    sltu s11, s4, s6
    sub s10, s6, s11

    sltu s11, s5, s6
    sub s10, s10, s11

    # d1
    sltu a6, s4, s7
    sub s11, s7, a6

    sltu a6, s5, s7
    sub s11, s11, a6

    sltu a6, s6, s7
    sub s11, s11, a6

    # permutation rank
    slli a6, s4, 2
    add s9, s9, a6

    slli a6, s4, 1
    add s9, s9, a6

    slli a6, s9, 2
    add s9, s9, a6

    add s9, s9, s10
    slli s9, s9, 2
    add s9, s9, s11

    # index = permutation * 81 + orientation
    slli a6, s9, 6
    add s8, s8, a6

    slli a6, s9, 4
    add s8, s8, a6

    add s8, s8, s9

    # nibble-packed PDB lookup
    srli s9, s8, 1
    add s9, s3, s9
    lbu s9, 0(s9)

    andi s10, s8, 1
    slli s10, s10, 2
    srl s9, s9, s10

    andi a6, s9, 15
    ret

# ============================================================
# print_move
#
# input:
#   a6 = move ID 0..8
#
# preserves:
#   a5, a6, t2, t3, t4, t5, t6, s0-s11
#
# clobbers:
#   a0, a7, t0, t1
# ============================================================

print_move:
    li t0, 3
    bltu a6, t0, print_R

    li t0, 6
    bltu a6, t0, print_B

    # D, D2, D'
    li a0, O_D
    addi t1, a6, -6
    j print_face

print_R:
    li a0, O_R
    mv t1, a6
    j print_face

print_B:
    li a0, O_B
    addi t1, a6, -3

print_face:
    li a7, 11
    ecall

    # No suffix
    beqz t1, print_move_done

    li t0, 1
    beq t1, t0, print_two

    # Prime suffix
    li a0, O_I
    j print_suffix

print_two:
    li a0, O_2

print_suffix:
    li a7, 11
    ecall

print_move_done:
    ret

.include "asm/data/tables.inc"
