#include "memory.h"
#include "klibc.h"
#include <stdint.h>
#include <stddef.h>

/* Simple heap allocator sitting right after the kernel.
   We use a linked-list of free/used blocks.            */

extern uint32_t kernel_end;   /* provided by linker script */

#define HEAP_START  0x200000   /* 2 MB */
#define HEAP_SIZE   (4 * 1024 * 1024)  /* 4 MB heap */

typedef struct block {
    uint32_t     magic;
    size_t       size;
    int          free;
    struct block *next;
    struct block *prev;
} block_t;

#define BLOCK_MAGIC  0xDEADC0DE
#define BLOCK_HDR    sizeof(block_t)

static block_t *heap_start_block = NULL;
static size_t   total_mem   = 0;
static size_t   used_mem    = 0;

static void heap_init(void) {
    heap_start_block = (block_t*)HEAP_START;
    heap_start_block->magic = BLOCK_MAGIC;
    heap_start_block->size  = HEAP_SIZE - BLOCK_HDR;
    heap_start_block->free  = 1;
    heap_start_block->next  = NULL;
    heap_start_block->prev  = NULL;
}

void pmm_init(multiboot_info_t *mbi) {
    if (mbi && (mbi->flags & (1 << 0))) {
        total_mem = (mbi->mem_upper + 1024) * 1024;
    } else {
        total_mem = 8 * 1024 * 1024;  /* assume 8 MB */
    }
    used_mem = 0;
    heap_init();
}

void *kmalloc(size_t size) {
    if (!size) return NULL;
    /* Align to 4 bytes */
    size = (size + 3) & ~3;

    block_t *b = heap_start_block;
    while (b) {
        if (b->free && b->size >= size && b->magic == BLOCK_MAGIC) {
            /* Split block if large enough */
            if (b->size >= size + BLOCK_HDR + 4) {
                block_t *nb = (block_t*)((uint8_t*)b + BLOCK_HDR + size);
                nb->magic = BLOCK_MAGIC;
                nb->size  = b->size - size - BLOCK_HDR;
                nb->free  = 1;
                nb->next  = b->next;
                nb->prev  = b;
                if (b->next) b->next->prev = nb;
                b->next   = nb;
                b->size   = size;
            }
            b->free = 0;
            used_mem += b->size + BLOCK_HDR;
            return (void*)((uint8_t*)b + BLOCK_HDR);
        }
        b = b->next;
    }
    return NULL;  /* OOM */
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t*)((uint8_t*)ptr - BLOCK_HDR);
    if (b->magic != BLOCK_MAGIC || b->free) return;
    b->free = 1;
    used_mem -= b->size + BLOCK_HDR;

    /* Merge with next free block */
    if (b->next && b->next->free) {
        b->size += BLOCK_HDR + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    /* Merge with prev free block */
    if (b->prev && b->prev->free) {
        b->prev->size += BLOCK_HDR + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

size_t kmem_total(void) { return total_mem; }
size_t kmem_used(void)  { return used_mem; }
size_t kmem_free_bytes(void) {
    size_t f = 0;
    block_t *b = heap_start_block;
    while (b) { if (b->free) f += b->size; b = b->next; }
    return f;
}
