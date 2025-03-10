#include "mm.h"
#include "defs.h"
#include "proc.h"
#include "stdlib.h"
#include "printk.h"
#include "elf.h"
#include "virtio.h"

//#define OFFSET(TYPE ,MEMBER)((unsigned long)(&(((TYPE *)0)->MEMBER)))

extern void __dummy();
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);

extern uint64_t  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern char _sramdisk[];
extern char _eramdisk[];

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 task_struct
struct task_struct *task[NR_TASKS]; // 线程数组，所有的线程都保存在此

static uint64_t load_program(struct task_struct *task) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)_sramdisk;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)(_sramdisk + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        Elf64_Phdr *phdr = phdrs + i;
        if (phdr->p_type == PT_LOAD) {
            /*uint64_t offset = (uint64_t)(phdr->p_vaddr)-PGROUNDDOWN(phdr->p_vaddr);
            uint64_t size = PGROUNDUP(phdr->p_memsz + offset) / PGSIZE;
            uint64_t ad = alloc_pages(size);
            uint64_t src_start = (uint64_t)_sramdisk + phdr->p_offset;
            for (int j = 0; j < phdr->p_memsz; ++j) {
                *((char*)ad + j + offset) = *((char*)src_start + j);
            }
            memset((void*)(ad + phdr->p_filesz + offset), 0, phdr->p_memsz-phdr->p_filesz);*/
            uint64_t perm = 0;
            perm |= (phdr->p_flags & 1) ? VM_EXEC : 0; //X
            perm |= (phdr->p_flags & 2) ? VM_WRITE : 0; //W
            perm |= (phdr->p_flags & 4) ? VM_READ : 0; //R
            //create_mapping(task->pgd, PGROUNDDOWN(phdr->p_vaddr), ad - PA2VA_OFFSET, phdr->p_memsz + offset, perm);
            //printk(" %lx, %lx",phdr->p_vaddr,phdr->p_vaddr+ phdr->p_memsz);
            do_mmap(&(task->mm), phdr->p_vaddr, phdr->p_memsz, phdr->p_offset, phdr->p_filesz, perm);
            // alloc space and copy content
            // do mapping
            // code...
        }
    }
    //uint64_t U_stack_top = alloc_page();
    //create_mapping(task->pgd, USER_END - PGSIZE, U_stack_top - PA2VA_OFFSET, PGSIZE, 23);
    task->thread.sepc = ehdr->e_entry;
    task->thread.sstatus = csr_read(sstatus);
    task->thread.sstatus &= ~(1 << 8);
    task->thread.sstatus |= (1 << 5);
    task->thread.sstatus |= (1 << 18);
    task->thread.sscratch = USER_END;
    return ehdr->e_entry;
}

void task_init() {
    srand(2024);

    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度，可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    idle = (struct task_struct*) kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;

    // 1. 参考 idle 的设置，为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，counter 和 priority 进行如下赋值：
    //     - counter  = 0;
    //     - priority = rand() 产生的随机数（控制范围在 [PRIORITY_MIN, PRIORITY_MAX] 之间）
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 thread_struct 中的 ra 和 sp
    //     - ra 设置为 __dummy（见 4.2.2）的地址
    //     - sp 设置为该线程申请的物理页的高地址

    for (int i = 1; i < 2; i++){
        task[i] = (struct task_struct*) kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand() % (PRIORITY_MAX - PRIORITY_MIN + 1) + PRIORITY_MIN;
        task[i]->pid = i;
        task[i]->thread.ra = (uint64_t) &__dummy;
        task[i]->thread.sp = (uint64_t) task[i] + PGSIZE;
        /*struct mm_struct *mm = (struct mm_struct *)kalloc();
        mm->mmap = NULL;
        task[i]->mm = *mm;*/
        task[i]->mm.mmap = NULL;
        task[i]->files = file_init();

        /*task[i]->thread.sepc = USER_START;
        task[i]->thread.sstatus = csr_read(sstatus);
        task[i]->thread.sstatus &= ~(1 << 8);//sret返回u-mode
        task[i]->thread.sstatus |= (1 << 5);//sret开启中断
        task[i]->thread.sstatus |= (1 << 18);//s-mode可以访问用户态进程页面
        task[i]->thread.sscratch = USER_END;*/

        task[i]->pgd = (uint64_t)alloc_page();
        for (int j = 0; j < 512; j++){
            task[i]->pgd[j] = swapper_pg_dir[j];
        }
        do_mmap(&(task[i]->mm), USER_END-PGSIZE, PGSIZE, 0, 0, VM_READ | VM_WRITE | VM_ANON);
        task[i]->thread.sepc = load_program(task[i]);

        /*uint64_t U_stack_top = kalloc();

        uint64_t size = PGROUNDUP((uint64_t)_eramdisk - (uint64_t)_sramdisk) / PGSIZE;
        uint64_t copy_addr = alloc_pages(size);
        for (int j = 0; j < _eramdisk - _sramdisk; ++j) 
            ((char *)copy_addr)[j] = _sramdisk[j];

        create_mapping(task[i]->pgd, USER_START, (uint64_t)copy_addr - PA2VA_OFFSET, size * PGSIZE, 31); // 映射用户段   U|X|W|R|V
        create_mapping(task[i]->pgd, USER_END - PGSIZE, U_stack_top - PA2VA_OFFSET, PGSIZE, 23); // 映射用户栈 U|-|W|R|V*/
    }

    virtio_dev_init();
    mbr_init();

    for (int i = 2; i < NR_TASKS; ++i) {
        task[i] = NULL;
    }

    printk("...task_init done!\n");
}

#if TEST_SCHED
#define MAX_OUTPUT ((NR_TASKS - 1) * 10)
char tasks_output[MAX_OUTPUT];
int tasks_output_index = 0;
char* expected_output = "2222222222111111133334222222222211111113";
#include "sbi.h"
#endif

void dummy() {
    uint64_t MOD = 1000000007;
    uint64_t auto_inc_local_var = 0;
    int last_counter = -1;
    //uint64_t iii = 0;
    while (1) {
        //printk("%d %d %d\n",current->pid,current->counter,++iii);
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if (current->counter == 1) {
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
            #if TEST_SCHED
            tasks_output[tasks_output_index++] = current->pid + '0';
            if (tasks_output_index == MAX_OUTPUT) {
                for (int i = 0; i < MAX_OUTPUT; ++i) {
                    if (tasks_output[i] != expected_output[i]) {
                        printk("\033[31mTest failed!\033[0m\n");
                        printk("\033[31m    Expected: %s\033[0m\n", expected_output);
                        printk("\033[31m    Got:      %s\033[0m\n", tasks_output);
                        sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
                    }
                }
                printk("\033[32mTest passed!\033[0m\n");
                printk("\033[32m    Output: %s\033[0m\n", expected_output);
                sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
            }
            #endif
        }
    }
}

extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next) {
    if (next == current){
        return;
    }else{
        struct task_struct *prev = current;
        current = next;
        __switch_to(prev, next);
    }
}

void do_timer() {
    // 1. 如果当前线程是 idle 线程或当前线程时间片耗尽则直接进行调度
    // 2. 否则对当前线程的运行剩余时间减 1，若剩余时间仍然大于 0 则直接返回，否则进行调度

    if (current == idle) {
        schedule();
    } else {
        --current->counter;
        if ((long)(current->counter) > 0) return;
        else {
            current->counter = 0;
            schedule();
        }
    }
}

void schedule() {
    int i,next,c;
    while (1){
        c = -1;
        next = 0;
        i = NR_TASKS;
        while (--i){
            if (!task[i]){
                continue;
            }
            if (task[i]->state == TASK_RUNNING && (long)(task[i]->counter) >= c){
                c = task[i]->counter;
                next = i;
            }
        }
        if (c){
            //printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n", next, task[next]->priority, task[next]->counter);
            break;
        }
        for (i = 1; i < NR_TASKS; i++){
            if (task[i]){
                task[i]->counter = task[i]->priority;
                //printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", i, task[i]->priority, task[i]->counter);
            }
        }
    }
    switch_to(task[next]);
}

struct vm_area_struct *find_vma(struct mm_struct *mm, uint64_t addr) {
    struct vm_area_struct *vma;
    for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
        //printk("%lx,%lx,%lx\n",vma->vm_start,vma->vm_end,addr);
        if (vma->vm_start <= addr && vma->vm_end >= addr) {
            return vma;
        }
    }
    return NULL;
}

void do_mmap(struct mm_struct *mm, uint64_t addr, uint64_t len, uint64_t vm_pgoff, uint64_t vm_filesz, uint64_t flags) {
    struct vm_area_struct *temp = (struct vm_area_struct *)kalloc();
    temp->vm_start = addr;
    temp->vm_end = addr + len;
    temp->vm_flags = flags;
    temp->vm_pgoff = vm_pgoff;
    temp->vm_filesz = vm_filesz;
    temp->vm_next = NULL;
    temp->vm_mm = mm;
    struct vm_area_struct *vma = mm->mmap;
    if (vma == NULL) {
        mm->mmap = temp;
        temp->vm_prev = NULL;
        return;
    }
    while (vma->vm_next != NULL) {
        vma = vma->vm_next;
    }
    vma->vm_next = temp;
    temp->vm_prev = vma;
    return;
}