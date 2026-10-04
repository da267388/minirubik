.global _start
_start:
    call main      # 跳去執行你的 C 程式入口
    li a7, 93      # Ripes / RISC-V Linux syscall 代碼 93 (exit)
    ecall          # 呼叫系統結束程式