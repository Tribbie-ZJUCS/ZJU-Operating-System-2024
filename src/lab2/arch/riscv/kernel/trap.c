#include "stdint.h"

void trap_handler(uint64_t scause, uint64_t sepc) {
    // 通过 `scause` 判断 trap 类型
    // 如果是 interrupt 判断是否是 timer interrupt
    if (scause >> 63 && scause % 8 == 5){
        // 如果是 timer interrupt 则打印输出相关信息，并通过 `clock_set_next_event()` 设置下一次时钟中断
        //printk("[S] Supervisor Mode Timer Interrupt\n");
        // `clock_set_next_event()` 见 4.3.4 节
        clock_set_next_event();
        do_timer();
    }
    // 其他 interrupt / exception 可以直接忽略，推荐打印出来供以后调试
}