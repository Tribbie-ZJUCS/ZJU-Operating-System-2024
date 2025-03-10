#include "syscall.h"
#include "printk.h"
#include "defs.h"
#include "mm.h"
#include "proc.h"
#include "fs.h"
#include <string.h>

extern struct task_struct* current;
extern void __ret_from_fork();
extern struct task_struct* task[NR_TASKS];
extern unsigned long swapper_pg_dir[512];
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);

int64_t sys_write(uint64_t fd, const char* buf, uint64_t len)
{
    uintptr_t ret;
    struct file *file = &(current->files->fd_array[fd]);
    if (file->opened == 0) {
        printk("file not opened\n");
        return ERROR_FILE_NOT_OPEN;
    } else {
        if (file->perms & FILE_WRITABLE) {
            ret = file->write(file, buf, len);
        }
    }
    return ret;
}

int64_t sys_read(uint64_t fd, char* buf, uint64_t len) {
    uint64_t ret;
    struct file *file = &(current->files->fd_array[fd]);
    if (file->opened == 0) {
        printk("file not opened\n");
        ret = ERROR_FILE_NOT_OPEN;
    } else {
        if (file->perms & FILE_READABLE) {
            ret = file->read(file, buf, len);
        }
    }
    return ret;
}

uint64_t sys_getpid()
{
    return current->pid;
}

uint64_t sys_clone(struct pt_regs* regs) {
    int pid = 0;
    while (pid < NR_TASKS) {
        if (!task[pid]) {
            break;
        }
        ++pid;
    }
    if (pid >= NR_TASKS) {
        return -1;
    }

    struct task_struct* child_task;
    child_task = (struct task_struct*)kalloc();
    task[pid] = child_task;
    for (int i = 0; i < 1<<12; i++) {
        ((char*)child_task)[i] = ((char*)current)[i];
    }
    child_task->pid = pid;
    child_task->thread.ra = (uint64_t)__ret_from_fork;

    uint64_t offset = (uint64_t)regs - (uint64_t)current;
    struct pt_regs *child_regs = (struct pt_regs *)((uint64_t)child_task + offset);
    child_regs->x[10] = 0;
    child_regs->x[2] = (uint64_t)child_regs;
    child_regs->sepc = regs->sepc + 4;
    child_task->thread.sp = (uint64_t)child_regs;
    child_task->thread.sscratch = csr_read(sscratch);//(uint64_t)child_regs;

    child_task->pgd = (uint64_t)alloc_page();
    memset(child_task->pgd, 0, 1 << 12);
    for (int j = 0; j < 512; j++) {
        child_task->pgd[j] = swapper_pg_dir[j];
    }
    child_task->mm.mmap = NULL;

    for (struct vm_area_struct *vma = current->mm.mmap; vma != NULL; vma = vma->vm_next) {
        do_mmap(&(child_task->mm), vma->vm_start, vma->vm_end - vma->vm_start, vma->vm_pgoff, vma->vm_filesz, vma->vm_flags);
        for (uint64_t vaddr = PGROUNDDOWN(vma->vm_start); vaddr < vma->vm_end; vaddr += PGSIZE) {
            if ((current->pgd[(vaddr >> 30) & 0x1ff] & (PTE_V)) == PTE_V) {
                uint64_t child_page = alloc_page();
                for (int j = 0; j < PGSIZE; j++) {
                    ((char *)child_page)[j] = ((char *)vaddr)[j];
                    create_mapping(child_task->pgd, vaddr, child_page-PA2VA_OFFSET, PGSIZE, vma->vm_flags | PTE_U | PTE_V);
                }
            }
        }
    }
    printk("[PID = %d] forked from [PID = %d]\n",child_task->pid,current->pid);
    return child_task->pid;
}

int64_t sys_openat(int dfd, const char *filename, int flags) {
    int fd = -1;
    
    for (int i = 0 ; i < MAX_FILE_NUMBER; i++) {
        if (current->files->fd_array[i].opened == 0) {
            fd = i;
            break;
        }
    }

    file_open(&(current->files->fd_array[fd]), filename, flags);

    return fd;
}

int64_t sys_lseek(int fd, int64_t offset, int whence) {
    int64_t ret;
    struct file* file = &(current->files->fd_array[fd]);
    if (file->opened) {
        if (file->perms & FILE_READABLE) {
            ret = file->lseek(file, offset, whence);
        }
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

int64_t sys_close(int fd) {
    int64_t ret;
    struct file* target_file = &(current->files->fd_array[fd]);
    if (target_file->opened) {
        target_file->opened = 0;
        ret = 0;
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}