// bench/bench_main.c
#include "solver_core.h"
#include "opcount.h"


// 宣告全域變數 (真正的記憶體配置)

// 建立查表 (Lookup Table)
const char* move_names[] = {
    "R", "R2", "R'", 
    "B", "B2", "B'", 
    "D", "D2", "D'"
};

uint8_t final_moves[12]; 


static inline void print_str_safe(const char* str) {
    int i = 0;
    // 嚴格檢查：只要不是 \0 就印出
    while (str[i] != '\0') {
        __asm__ volatile (
            "li a7, 11\n\t"     // ecall 11: 僅印出單一字元
            "mv a0, %0\n\t"
            "ecall"
            : /* 無回傳值 */
            : "r" (str[i])
            : "a0", "a7"
        );
        i++;
    }
}

// 替代 printf("%c", c) 的底層實作 (使用 Ripes ecall 11)
static inline void print_char(char c) {
    __asm__ volatile (
        "li a7, 11\n\t"
        "mv a0, %0\n\t"
        "ecall"
        : 
        : "r" (c)
        : "a0", "a7"
    );
}



int main() {
    const char *test_case = "14325671111111";
    
#ifdef OPCOUNT
    // 初始化計數器 (在裸機環境中，確保 .bss 被清零或手動歸零)
    opc.iters = 0; opc.children = 0; opc.descended = 0; opc.hlook = 0; opc.idx = 0;
#endif

    int path_length = solve(test_case, final_moves);
    for (int i = 0; i < path_length; i++) {
        print_str_safe(move_names[final_moves[i]]);
        print_char(' '); // 印出空白分隔
    }
    print_char('\n');    // 換行

    // 將「實際展開的節點數」強轉為 32-bit int 傳給 a0
    // (對於最壞情況 11 children 數量在十萬級別，32-bit 絕對夠裝)
#ifdef OPCOUNT
    return (int)opc.children;
#else
    return 0;
#endif
}