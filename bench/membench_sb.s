# membench_sb.s — write NBYTES distinct 8-bit bytes, then exit
.equ BASE,   0x20000000      # 起點:避開 .data、stack、I/O 區,實際位址請對照你的 Ripes 記憶體映射
.equ NBYTES, 262144          # 1 MiB;每個量測點用 sed 換掉這個值

.text
main:
    li   t0, BASE
    li   t1, NBYTES
    li   t2, 0x5A5A5A5A      # 非零,避免任何「寫零省略」的最佳化
    beqz t1, done            # 控制組 NBYTES=0 時直接結束
loop:
    sb   t2, 0(t0)
    addi t0, t0, 1
    addi t1, t1, -1
    bnez t1, loop
done:
    li   a7, 10              # Ripes ecall:結束
    ecall