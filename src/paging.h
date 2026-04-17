#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE        0x1000
#define PAGE_ENTRIES     1024
#define PAGE_ALIGN(a)    (((a) + PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PAGE_DIR_IDX(a)  ((a) >> 22)
#define PAGE_TBL_IDX(a)  (((a) >> 12) & 0x3FF)

#define PAGE_PRESENT     (1 << 0)
#define PAGE_WRITE       (1 << 1)
#define PAGE_USER        (1 << 2)
#define PAGE_ACCESSED    (1 << 5)
#define PAGE_DIRTY       (1 << 6)
#define PAGE_COW         (1 << 9)

#define KERN_BASE        0x00100000
#define HEAP_VIRT_BASE   0x00200000
#define HEAP_VIRT_MAX    0x01000000
#define VMALLOC_BASE     0x01000000
#define VMALLOC_END      0x02000000
#define USER_BASE        0x40000000

void     pmm_init(uint32_t mem_kb);
uint32_t pmm_alloc(void);
void     pmm_free(uint32_t addr);
uint32_t pmm_used(void);
uint32_t pmm_total(void);
void     pmm_ref(uint32_t addr);
uint32_t pmm_refcount(uint32_t addr);

void     paging_init(uint32_t mem_kb);
void     paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
void     paging_unmap(uint32_t virt);
uint32_t paging_virt_to_phys(uint32_t virt);
int      paging_is_mapped(uint32_t virt);
void     paging_dump_range(uint32_t start, uint32_t end);

void     demand_paging_init(void);
uint32_t demand_fault_count(void);
uint32_t demand_cow_count(void);

void     heap_init(void);
void    *heap_alloc(uint32_t size);
void     heap_free(void *ptr);
uint32_t heap_brk(void);
uint32_t heap_used(void);
uint32_t heap_capacity(void);

void    *vmalloc(uint32_t size);
void     vmfree(void *ptr);
int      vmalloc_copy_on_write(uint32_t virt);

#endif