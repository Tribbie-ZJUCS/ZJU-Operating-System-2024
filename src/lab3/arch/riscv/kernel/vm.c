#include "defs.h"
#include "string.h"
#include "mm.h"

/* early_pgtbl: 用于 setup_vm 进行 1GiB 的映射 */
uint64_t early_pgtbl[512] __attribute__((__aligned__(0x1000)));
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);

void setup_vm() {
    /* 
     * 1. 由于是进行 1GiB 的映射，这里不需要使用多级页表 
     * 2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
     *     high bit 可以忽略
     *     中间 9 bit 作为 early_pgtbl 的 index
     *     低 30 bit 作为页内偏移，这里注意到 30 = 9 + 9 + 12，即我们只使用根页表，根页表的每个 entry 都对应 1GiB 的区域
     * 3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    **/
   memset(early_pgtbl, 0, PGSIZE);
   early_pgtbl[PHY_START >> 30 & 0x1ff] = (PHY_START >> 30 & 0x3ffffff) << 28 | 15;
   early_pgtbl[VM_START >> 30 & 0x1ff] = (PHY_START >> 30 & 0x3ffffff) << 28 | 15;
   return;
}

/* swapper_pg_dir: kernel pagetable 根目录，在 setup_vm_final 进行映射 */
uint64_t swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern uint64_t _skernel,_stext, _srodata, _sdata, _sbss;
void setup_vm_final() {
    memset(swapper_pg_dir, 0x0, PGSIZE);

    // No OpenSBI mapping required

    // mapping kernel text X|-|R|V
    create_mapping(swapper_pg_dir, (uint64_t)&_stext, (uint64_t)&_stext - PA2VA_OFFSET, (uint64_t)&_srodata - (uint64_t)&_stext, 11);

    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir, (uint64_t)&_srodata, (uint64_t)&_srodata - PA2VA_OFFSET, (uint64_t)&_sdata - (uint64_t)&_srodata, 3);

    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir, (uint64_t)&_sdata, (uint64_t)&_sdata - PA2VA_OFFSET, PHY_SIZE - ((uint64_t)&_sdata - (uint64_t)&_stext), 7);

    // set satp with swapper_pg_dir

    // YOUR CODE HERE
    asm volatile (
        "mv t0, %[swapper_pg_dir]\n"
        ".set _pa2va_, 0xffffffdf80000000\n"
        "li t1, _pa2va_\n"
        "sub t0, t0, t1\n"
        "srli t0, t0, 12\n"
        "addi t2, zero, 1\n"
        "li t1, 63\n"
        "sll t2, t2, t1\n"
        "or t0, t0, t2\n"

        "csrw satp, t0\n"

        : : [swapper_pg_dir] "r" (swapper_pg_dir)
        : "memory"        
    );

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");
    return;
}


/* 创建多级页表映射关系 */
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm) {
    /*
     * pgtbl 为根页表的基地址
     * va, pa 为需要映射的虚拟地址、物理地址
     * sz 为映射的大小，单位为字节
     * perm 为映射的权限（即页表项的低 8 位）
     * 
     * 创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
     * 可以使用 V bit 来判断页表项是否存在
    **/
    uint64_t VPN[3];
    uint64_t *page_table[3];
    uint64_t new_page;

    for (uint64_t address = va; address < va + sz; address += PGSIZE, pa += PGSIZE){
        page_table[2] = pgtbl;
        VPN[2] = (address >> 30) & 0x1ff;
        VPN[1] = (address >> 21) & 0x1ff;
        VPN[0] = (address >> 12) & 0x1ff;
        for (int level = 2; level > 0; level--){
            if ((page_table[level][VPN[level]] & 1) == 0){
                new_page = kalloc();
                page_table[level][VPN[level]] = (((new_page - PA2VA_OFFSET) >> 12) << 10) | 1;
            }
            page_table[level - 1] = (uint64_t*)(((page_table[level][VPN[level]] >> 10) << 12) + PA2VA_OFFSET);
        }
        page_table[0][VPN[0]] = ((pa >> 12) << 10) | (perm & 0x3ff);
    }
}