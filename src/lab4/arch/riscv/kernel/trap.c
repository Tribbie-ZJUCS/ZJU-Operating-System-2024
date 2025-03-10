#include "printk.h"
#include "stdint.h"
#include "syscall.h"

struct pt_regs{
    uint64_t x[32];
    uint64_t sepc;
    uint64_t sstatus;
};

void trap_handler(uint64_t scause, uint64_t sepc, struct pt_regs *regs) {
    // 通过 `scause` 判断 trap 类型
    // 如果是 interrupt 判断是否是 timer interrupt
    if (scause >> 63 && scause % 8 == 5){
        // 如果是 timer interrupt 则打印输出相关信息，并通过 `clock_set_next_event()` 设置下一次时钟中断
        //printk("[S] Supervisor Mode Timer Interrupt\n");
        // `clock_set_next_event()` 见 4.3.4 节
        clock_set_next_event();
        do_timer();
    } else if (scause == 8) {
        uint64_t ret;
        if (regs->x[17] == SYS_WRITE) {
            ret = sys_write((unsigned int)(regs->x[10]), (const char*)(regs->x[11]), (size_t)(regs->x[12]));
            regs->sepc += 4;
        }
        else if (regs->x[17] == SYS_GETPID) {
            ret = sys_getpid();
            regs->sepc += 4;
        }
        regs->x[10] = ret;
    }
    // 其他 interrupt / exception 可以直接忽略，推荐打印出来供以后调试
}