#pragma once
#include <stddef.h>
#include <stdint.h>

/* Multiboot memory map entry */
typedef struct {
    uint32_t size;
    uint32_t base_low, base_high;
    uint32_t len_low,  len_high;
    uint32_t type;
} __attribute__((packed)) mmap_entry_t;

/* Multiboot info struct (subset we use) */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

void   pmm_init(multiboot_info_t *mbi);
void  *kmalloc(size_t size);
void   kfree(void *ptr);
size_t kmem_total(void);
size_t kmem_used(void);
size_t kmem_free_bytes(void);
