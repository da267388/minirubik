// bench/bench_main.c
#include "solver_core.h"
#include "opcount.h"


// 宣告全域變數 (真正的記憶體配置)

uint8_t final_moves[12]; 

int main() {
    const char *test_case = "21345671111111";
    
#ifdef OPCOUNT
    // 初始化計數器 (在裸機環境中，確保 .bss 被清零或手動歸零)
    opc.iters = 0; opc.children = 0; opc.descended = 0; opc.hlook = 0; opc.idx = 0;
#endif

    solve(test_case, final_moves);

    // 將「實際展開的節點數」強轉為 32-bit int 傳給 a0
    // (對於最壞情況 11 步，descended 數量在十萬級別，32-bit 絕對夠裝)
#ifdef OPCOUNT
    return (int)opc.hlook;
#else
    return 0;
#endif
}