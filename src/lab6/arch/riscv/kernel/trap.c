#include "printk.h"
#include "stdint.h"
#include "syscall.h"
#include "defs.h"
#include "proc.h"

extern char _sramdisk[];
extern char _eramdisk[];


void do_page_fault(struct pt_regs *regs) {
    uint64_t stval = csr_read(stval);
    struct vm_area_struct *pgf_vm_area = find_vma(&(current->mm), stval);
    if (pgf_vm_area == NULL){
        Err("illegal page fault");
    }
    uint64_t va = PGROUNDDOWN(stval);
    uint64_t sz = PGROUNDUP(stval + PGSIZE) - va;
    uint64_t pa = alloc_pages(sz / PGSIZE);
    uint64_t perm = !!(pgf_vm_area->vm_flags & VM_READ) * PTE_R |
                    !!(pgf_vm_area->vm_flags & VM_WRITE) * PTE_W |
                    !!(pgf_vm_area->vm_flags & VM_EXEC) * PTE_X |
                    PTE_U | PTE_V;
    memset((void*)pa, 0, sz);

    if (pgf_vm_area->vm_flags & VM_ANON) {

    } else {
        uint64_t src = (uint64_t)_sramdisk + pgf_vm_area->vm_pgoff;
        uint64_t offset = stval - pgf_vm_area->vm_start;
        uint64_t src_uapp = PGROUNDDOWN(src + offset);
        for (int j = 0; j < sz; j++) {
            ((char*)(pa))[j] = ((char*)src_uapp)[j];
        }
    }

    create_mapping(current->pgd, va, pa-PA2VA_OFFSET, sz, perm);
}

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
            ret = (uint64_t)sys_write((uint64_t)(regs->x[10]), (const char*)(regs->x[11]), (uint64_t)(regs->x[12]));
            regs->sepc += 4;
        }
        else if (regs->x[17] == SYS_READ) {
            ret = (uint64_t)sys_read((uint64_t)(regs->x[10]), (const char*)(regs->x[11]), (uint64_t)(regs->x[12]));
            regs->sepc += 4;
        } 
        else if (regs->x[17] == SYS_LSEEK) {
            ret = (uint64_t)sys_lseek((int)(regs->x[10]), (int64_t)(regs->x[11]), (int)(regs->x[12]));
            regs->sepc += 4;
        }
        else if (regs->x[17] == SYS_CLOSE) {
            ret = (uint64_t)sys_close((int)(regs->x[10]));
            regs->sepc += 4;
        }
        else if (regs->x[17] == SYS_OPENAT) {
            ret = (uint64_t)sys_openat((int)(regs->x[10]), (const char*)(regs->x[11]), (int)(regs->x[12]));
            regs->sepc += 4;
        }
        else if (regs->x[17] == SYS_GETPID) {
            ret = sys_getpid();
            regs->sepc += 4;
        }
        else if (regs->x[17] == SYS_CLONE) {
            ret = sys_clone(regs);
            regs->sepc += 4;
        }
        regs->x[10] = ret;
    } else if (scause == 12) {//instruction page fault
        //printk("scause = %d, sepc = %lx, stval = %lx\n",csr_read(scause), csr_read(sepc), csr_read(stval));
        do_page_fault(regs);
    } else if (scause == 13) {//load page fault
        //printk("scause = %d, sepc = %lx, stval = %lx\n",csr_read(scause), csr_read(sepc), csr_read(stval));
        do_page_fault(regs);
    } else if (scause == 15) {//store page fault
        //printk("scause = %d, sepc = %lx, stval = %lx\n",csr_read(scause), csr_read(sepc), csr_read(stval));
        do_page_fault(regs);
    }
    // 其他 interrupt / exception 可以直接忽略，推荐打印出来供以后调试
}