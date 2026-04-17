#include "kmalloc.h"
#include "kstring.h"

#define MAX_BLOCKS 256

typedef struct {
    uint32_t addr;
    uint32_t size;
    int      used;
} block_t;

static block_t blocks[MAX_BLOCKS];
static int     num_blocks = 0;
static uint32_t heap_start = 0;
static uint32_t heap_size  = 0;
static uint32_t heap_ptr   = 0;

void kmalloc_init(uint32_t start, uint32_t size) {
    heap_start = start;
    heap_size  = size;
    heap_ptr   = start;
    num_blocks = 0;
    kmemset(blocks, 0, sizeof(blocks));
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    size = (size + 3) & ~3;
    if (heap_ptr + size > heap_start + heap_size) return 0;

    for (int i = 0; i < num_blocks; i++) {
        if (!blocks[i].used && blocks[i].size >= size) {
            blocks[i].used = 1;
            return (void *)(uint32_t)blocks[i].addr;
        }
    }

    if (num_blocks >= MAX_BLOCKS) return 0;

    blocks[num_blocks].addr = heap_ptr;
    blocks[num_blocks].size = size;
    blocks[num_blocks].used = 1;
    num_blocks++;
    heap_ptr += size;
    return (void *)(heap_ptr - size);
}

void kfree(void *ptr) {
    uint32_t addr = (uint32_t)ptr;
    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].addr == addr) {
            blocks[i].used = 0;
            return;
        }
    }
}

uint32_t kmalloc_used(void) {
    uint32_t used = 0;
    for (int i = 0; i < num_blocks; i++)
        if (blocks[i].used) used += blocks[i].size;
    return used;
}

uint32_t kmalloc_free(void) {
    return heap_size - (heap_ptr - heap_start);
}