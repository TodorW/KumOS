
#include "paging.h"
#include "idt.h"
#include "vga.h"
#include "kstring.h"
#include "swap.h"
#include <stdint.h>

#define PMM_MAX_FRAMES  65536

static uint8_t  pmm_refs[PMM_MAX_FRAMES];
static uint32_t pmm_total_frames = 0;
static uint32_t pmm_used_frames  = 0;
static uint32_t pmm_base_frame   = 0;

static inline uint32_t frame_idx(uint32_t phys) { return phys / PAGE_SIZE; }

void pmm_init(uint32_t mem_kb) {
    kmemset(pmm_refs, 0xFF, sizeof(pmm_refs));
    pmm_used_frames = 0;

    uint32_t start = 0x00100000;
    uint32_t end   = mem_kb * 1024;
    if (end > PMM_MAX_FRAMES * PAGE_SIZE)
        end = PMM_MAX_FRAMES * PAGE_SIZE;

    pmm_base_frame   = frame_idx(start);
    pmm_total_frames = (end - start) / PAGE_SIZE;
    if (pmm_total_frames > PMM_MAX_FRAMES - pmm_base_frame)
        pmm_total_frames = PMM_MAX_FRAMES - pmm_base_frame;

    for (uint32_t i = 0; i < pmm_total_frames; i++)
        pmm_refs[pmm_base_frame + i] = 0;

    uint32_t kern_end = frame_idx(0x00500000);
    for (uint32_t f = pmm_base_frame; f < kern_end && f < PMM_MAX_FRAMES; f++) {
        pmm_refs[f] = 1;
        pmm_used_frames++;
    }
}

uint32_t pmm_alloc(void) {
    for (uint32_t i = pmm_base_frame; i < pmm_base_frame + pmm_total_frames; i++) {
        if (i >= PMM_MAX_FRAMES) break;
        if (pmm_refs[i] == 0) {
            pmm_refs[i] = 1;
            pmm_used_frames++;
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free(uint32_t addr) {
    uint32_t idx = frame_idx(addr);
    if (idx >= PMM_MAX_FRAMES) return;
    if (pmm_refs[idx] == 0) return;
    pmm_refs[idx]--;
    if (pmm_refs[idx] == 0 && pmm_used_frames)
        pmm_used_frames--;
}

void pmm_ref(uint32_t addr) {
    uint32_t idx = frame_idx(addr);
    if (idx < PMM_MAX_FRAMES && pmm_refs[idx] < 254)
        pmm_refs[idx]++;
}

uint32_t pmm_refcount(uint32_t addr) {
    uint32_t idx = frame_idx(addr);
    return (idx < PMM_MAX_FRAMES) ? pmm_refs[idx] : 0;
}

uint32_t pmm_used(void)  { return pmm_used_frames;  }
uint32_t pmm_total(void) { return pmm_total_frames; }

static uint32_t page_dir[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

#define NUM_PREINIT_TABLES 4
static uint32_t kern_tables[NUM_PREINIT_TABLES][PAGE_ENTRIES]
                            __attribute__((aligned(PAGE_SIZE)));

static inline void load_cr3(uint32_t dir) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(dir) : "memory");
}
static inline void tlb_flush_page(uint32_t v) {
    __asm__ volatile ("invlpg (%0)" :: "r"(v) : "memory");
}
static inline void enable_paging(void) {
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

static uint32_t *pte_ptr(uint32_t virt, int alloc) {
    uint32_t di = PAGE_DIR_IDX(virt);
    uint32_t ti = PAGE_TBL_IDX(virt);

    if (!(page_dir[di] & PAGE_PRESENT)) {
        if (!alloc) return 0;
        uint32_t phys = pmm_alloc();
        if (!phys) return 0;
        kmemset((void *)phys, 0, PAGE_SIZE);
        page_dir[di] = phys | PAGE_PRESENT | PAGE_WRITE;
    }

    uint32_t *tbl = (uint32_t *)(page_dir[di] & ~0xFFF);
    return &tbl[ti];
}

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t *pte = pte_ptr(virt, 1);
    if (!pte) return;
    *pte = (phys & ~0xFFF) | PAGE_PRESENT | flags;
    tlb_flush_page(virt);
}

void paging_unmap(uint32_t virt) {
    uint32_t *pte = pte_ptr(virt, 0);
    if (!pte || !(*pte & PAGE_PRESENT)) return;
    pmm_free(*pte & ~0xFFF);
    *pte = 0;
    tlb_flush_page(virt);
}

uint32_t paging_virt_to_phys(uint32_t virt) {
    uint32_t *pte = pte_ptr(virt, 0);
    if (!pte || !(*pte & PAGE_PRESENT)) return 0;
    return (*pte & ~0xFFF) | (virt & 0xFFF);
}

int paging_is_mapped(uint32_t virt) {
    uint32_t *pte = pte_ptr(virt, 0);
    return pte && (*pte & PAGE_PRESENT);
}

void paging_dump_range(uint32_t start, uint32_t end) {
    uint32_t addr = start & ~0xFFF;
    int in_run = 0;
    uint32_t run_start = 0, run_phys = 0;
    while (addr < end) {
        if (paging_is_mapped(addr)) {
            if (!in_run) { in_run=1; run_start=addr; run_phys=paging_virt_to_phys(addr); }
        } else {
            if (in_run) {
                vga_puts("    "); vga_put_hex(run_start);
                vga_puts(" → "); vga_put_hex(run_phys);
                vga_puts(" ("); vga_put_dec((addr-run_start)/1024); vga_puts(" KB)\n");
                in_run = 0;
            }
        }
        addr += PAGE_SIZE;
    }
    if (in_run) {
        vga_puts("    "); vga_put_hex(run_start);
        vga_puts(" → "); vga_put_hex(run_phys);
        vga_puts(" ("); vga_put_dec((addr-run_start)/1024); vga_puts(" KB)\n");
    }
}

void paging_init(uint32_t mem_kb) {
    pmm_init(mem_kb);
    kmemset(page_dir, 0, sizeof(page_dir));

    for (int t = 0; t < NUM_PREINIT_TABLES; t++) {
        kmemset(kern_tables[t], 0, PAGE_SIZE);
        for (int p = 0; p < PAGE_ENTRIES; p++) {
            uint32_t phys = (uint32_t)(t * PAGE_ENTRIES + p) * PAGE_SIZE;
            kern_tables[t][p] = phys | PAGE_PRESENT | PAGE_WRITE;
        }
        page_dir[t] = (uint32_t)kern_tables[t] | PAGE_PRESENT | PAGE_WRITE;
    }

    load_cr3((uint32_t)page_dir);
    enable_paging();
}

#define HEAP_MAX_BLOCKS  512

typedef struct {
    uint32_t addr;
    uint32_t size;
    int      used;
} hblock_t;

static hblock_t  hblocks[HEAP_MAX_BLOCKS];
static int       hnum_blocks = 0;
static uint32_t  heap_brk_ptr = HEAP_VIRT_BASE;
static uint32_t  heap_bytes_used = 0;

void heap_init(void) {
    kmemset(hblocks, 0, sizeof(hblocks));
    hnum_blocks    = 0;
    heap_brk_ptr   = HEAP_VIRT_BASE;
    heap_bytes_used = 0;

}

static int heap_grow(uint32_t size) {
    uint32_t new_brk = PAGE_ALIGN(heap_brk_ptr + size);
    if (new_brk >= HEAP_VIRT_MAX) return 0;

    uint32_t addr = PAGE_ALIGN(heap_brk_ptr);
    while (addr < new_brk) {
        if (!paging_is_mapped(addr)) {
            uint32_t phys = pmm_alloc();
            if (!phys) return 0;
            paging_map(addr, phys, PAGE_WRITE);
            kmemset((void *)addr, 0, PAGE_SIZE);
        }
        addr += PAGE_SIZE;
    }
    heap_brk_ptr = new_brk;
    return 1;
}

void *heap_alloc(uint32_t size) {
    if (!size) return 0;
    size = (size + 7) & ~7;

    for (int i = 0; i < hnum_blocks; i++) {
        if (!hblocks[i].used && hblocks[i].size >= size) {
            hblocks[i].used = 1;
            heap_bytes_used += hblocks[i].size;
            return (void *)hblocks[i].addr;
        }
    }

    if (hnum_blocks >= HEAP_MAX_BLOCKS) return 0;

    uint32_t addr = heap_brk_ptr;
    if (!heap_grow(size)) return 0;

    hblocks[hnum_blocks].addr = addr;
    hblocks[hnum_blocks].size = size;
    hblocks[hnum_blocks].used = 1;
    hnum_blocks++;
    heap_bytes_used += size;
    return (void *)addr;
}

void heap_free(void *ptr) {
    uint32_t addr = (uint32_t)ptr;
    for (int i = 0; i < hnum_blocks; i++) {
        if (hblocks[i].addr == addr && hblocks[i].used) {
            hblocks[i].used = 0;
            if (heap_bytes_used >= hblocks[i].size)
                heap_bytes_used -= hblocks[i].size;
            return;
        }
    }
}

uint32_t heap_brk(void)      { return heap_brk_ptr;    }
uint32_t heap_used(void)     { return heap_bytes_used;  }
uint32_t heap_capacity(void) { return heap_brk_ptr - HEAP_VIRT_BASE; }

#define VMALLOC_MAX_REGIONS  64

typedef struct {
    uint32_t virt_start;
    uint32_t size;
    int      active;
} vregion_t;

static vregion_t vregions[VMALLOC_MAX_REGIONS];
static int       vnum_regions = 0;
static uint32_t  vmalloc_ptr  = VMALLOC_BASE;

void *vmalloc(uint32_t size) {
    if (!size || vnum_regions >= VMALLOC_MAX_REGIONS) return 0;
    size = PAGE_ALIGN(size);
    if (vmalloc_ptr + size >= VMALLOC_END) return 0;

    uint32_t virt = vmalloc_ptr;
    for (uint32_t off = 0; off < size; off += PAGE_SIZE) {
        uint32_t phys = pmm_alloc();
        if (!phys) {

            for (uint32_t r = 0; r < off; r += PAGE_SIZE)
                paging_unmap(virt + r);
            return 0;
        }
        paging_map(virt + off, phys, PAGE_WRITE);
        kmemset((void *)(virt + off), 0, PAGE_SIZE);
    }

    vregions[vnum_regions].virt_start = virt;
    vregions[vnum_regions].size       = size;
    vregions[vnum_regions].active     = 1;
    vnum_regions++;
    vmalloc_ptr += size;
    return (void *)virt;
}

void vmfree(void *ptr) {
    uint32_t virt = (uint32_t)ptr;
    for (int i = 0; i < vnum_regions; i++) {
        if (vregions[i].active && vregions[i].virt_start == virt) {
            for (uint32_t off = 0; off < vregions[i].size; off += PAGE_SIZE)
                paging_unmap(virt + off);
            vregions[i].active = 0;
            return;
        }
    }
}

int vmalloc_copy_on_write(uint32_t virt) {

    uint32_t *pte = pte_ptr(virt & ~0xFFF, 0);
    if (!pte || !(*pte & PAGE_PRESENT)) return 0;
    uint32_t phys = *pte & ~0xFFF;
    pmm_ref(phys);
    *pte = (*pte & ~PAGE_WRITE) | PAGE_COW;
    tlb_flush_page(virt & ~0xFFF);
    return 1;
}

static volatile uint32_t fault_count = 0;
static volatile uint32_t cow_count   = 0;

static void page_fault_handler(registers_t *r) {
    uint32_t fault_addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));

    int present  = r->err_code & 0x1;
    int write    = r->err_code & 0x2;

    if (!present && fault_addr >= HEAP_VIRT_BASE && fault_addr < HEAP_VIRT_MAX) {
        uint32_t page = fault_addr & ~0xFFF;
        uint32_t phys = pmm_alloc();
        if (phys) {
            paging_map(page, phys, PAGE_WRITE);
            kmemset((void *)page, 0, PAGE_SIZE);
            fault_count++;
            return;
        }

    }

    if (present && write) {
        uint32_t page = fault_addr & ~0xFFF;
        uint32_t *pte = pte_ptr(page, 0);
        if (pte && (*pte & PAGE_COW)) {
            uint32_t old_phys = *pte & ~0xFFF;
            if (pmm_refcount(old_phys) > 1) {

                uint32_t new_phys = pmm_alloc();
                if (new_phys) {
                    kmemcpy((void *)new_phys, (void *)old_phys, PAGE_SIZE);
                    pmm_free(old_phys);
                    *pte = new_phys | PAGE_PRESENT | PAGE_WRITE;
                    tlb_flush_page(page);
                    cow_count++;
                    fault_count++;
                    return;
                }
            } else {

                *pte = (*pte & ~PAGE_COW) | PAGE_WRITE;
                tlb_flush_page(page);
                cow_count++;
                fault_count++;
                return;
            }
        }
    }

    if (!present) {
        uint32_t page = fault_addr & ~0xFFF;
        if (swap_is_swapped(page)) {
            if (swap_in(page) == 0) {
                fault_count++;
                return;
            }
        }
    }

    vga_fill_rect(0,0,80,1,' ',VGA_WHITE,VGA_RED);
    vga_puts_at("PAGE FAULT  CR2=", 0, 0, VGA_YELLOW, VGA_RED);
    char buf[12]; kitoa(fault_addr, buf, 16);
    vga_puts_at(buf, 17, 0, VGA_WHITE, VGA_RED);
    vga_puts_at(write ? " WRITE" : " READ", 28, 0, VGA_WHITE, VGA_RED);

    exc_register(14, 0);

    __asm__ volatile ("cli; hlt");
    while(1);
}

void demand_paging_init(void) {
    heap_init();
    exc_register(14, page_fault_handler);
}

uint32_t demand_fault_count(void) { return fault_count; }
uint32_t demand_cow_count(void)   { return cow_count;   }
uint32_t paging_current_dir(void) {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    return cr3;
}

void paging_switch(uint32_t dir_phys) {
    load_cr3(dir_phys);
}

uint32_t paging_clone_dir(void) {
    uint32_t new_dir_phys = pmm_alloc();
    if (!new_dir_phys) return 0;
    uint32_t *new_dir = (uint32_t *)new_dir_phys;
    kmemset(new_dir, 0, PAGE_SIZE);

    for (int i = 0; i < PAGE_ENTRIES; i++) {
        if (!page_dir[i]) continue;
        if (i < 256) {
            new_dir[i] = page_dir[i];
        } else {
            uint32_t tbl_phys = page_dir[i] & ~0xFFF;
            uint32_t *src_tbl = (uint32_t *)tbl_phys;
            uint32_t new_tbl_phys = pmm_alloc();
            if (!new_tbl_phys) { pmm_free(new_dir_phys); return 0; }
            uint32_t *new_tbl = (uint32_t *)new_tbl_phys;
            kmemset(new_tbl, 0, PAGE_SIZE);
            for (int j = 0; j < PAGE_ENTRIES; j++) {
                if (!src_tbl[j]) continue;
                uint32_t phys = src_tbl[j] & ~0xFFF;
                uint32_t flags = src_tbl[j] & 0xFFF;
                uint32_t new_phys = pmm_alloc();
                if (!new_phys) continue;
                kmemcpy((void *)new_phys, (void *)phys, PAGE_SIZE);
                new_tbl[j] = new_phys | flags;
            }
            new_dir[i] = new_tbl_phys | (page_dir[i] & 0xFFF);
        }
    }
    return new_dir_phys;
}

void paging_free_user(uint32_t dir_phys) {
    uint32_t *dir = (uint32_t *)dir_phys;
    for (int i = 256; i < PAGE_ENTRIES; i++) {
        if (!dir[i]) continue;
        uint32_t tbl_phys = dir[i] & ~0xFFF;
        uint32_t *tbl = (uint32_t *)tbl_phys;
        for (int j = 0; j < PAGE_ENTRIES; j++) {
            if (tbl[j]) pmm_free(tbl[j] & ~0xFFF);
        }
        pmm_free(tbl_phys);
    }
    pmm_free(dir_phys);
}

void paging_copy_user_range(uint32_t src_dir, uint32_t dst_dir,
                             uint32_t start, uint32_t end) {
    (void)src_dir; (void)dst_dir; (void)start; (void)end;
}
