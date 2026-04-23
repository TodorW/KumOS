#include "kmalloc.h"
#include "kstring.h"

#define MAGIC_FREE  0xDEADBEEF
#define MAGIC_USED  0xCAFEBABE
#define MIN_SPLIT   16

typedef struct block {
    uint32_t      magic;
    uint32_t      size;
    struct block *prev;
    struct block *next;
} block_t;

static block_t *free_list  = 0;
static uint32_t heap_start = 0;
static uint32_t heap_end   = 0;
static uint32_t heap_cap   = 0;

void kmalloc_init(uint32_t start, uint32_t size) {
    heap_start = start;
    heap_end   = start;
    heap_cap   = start + size;
    free_list  = 0;
}

static block_t *extend_heap(uint32_t need) {
    uint32_t total = sizeof(block_t) + need;
    if (heap_end + total > heap_cap) return 0;
    block_t *b = (block_t *)heap_end;
    b->magic = MAGIC_FREE;
    b->size  = need;
    b->prev  = 0;
    b->next  = free_list;
    if (free_list) free_list->prev = b;
    free_list = b;
    heap_end += total;
    return b;
}

static void coalesce(block_t *b) {
    block_t *next = (block_t *)((uint8_t *)b + sizeof(block_t) + b->size);
    if ((uint32_t)next < heap_end && next->magic == MAGIC_FREE) {
        b->size += sizeof(block_t) + next->size;
        if (next->prev) next->prev->next = next->next;
        else free_list = next->next;
        if (next->next) next->next->prev = next->prev;
    }
}

void *kmalloc(size_t size) {
    if (!size) return 0;
    size = (size + 7) & ~7u;

    for (block_t *b = free_list; b; b = b->next) {
        if (b->size < size) continue;
        if (b->size >= size + sizeof(block_t) + MIN_SPLIT) {
            block_t *split = (block_t *)((uint8_t *)b + sizeof(block_t) + size);
            split->magic = MAGIC_FREE;
            split->size  = b->size - size - sizeof(block_t);
            split->prev  = b->prev;
            split->next  = b->next;
            if (b->prev) b->prev->next = split; else free_list = split;
            if (b->next) b->next->prev = split;
            b->size = size;
        } else {
            if (b->prev) b->prev->next = b->next; else free_list = b->next;
            if (b->next) b->next->prev = b->prev;
        }
        b->magic = MAGIC_USED;
        b->prev = b->next = 0;
        return (void *)((uint8_t *)b + sizeof(block_t));
    }

    block_t *b = extend_heap(size);
    if (!b) return 0;
    if (b->prev) b->prev->next = b->next; else free_list = b->next;
    if (b->next) b->next->prev = b->prev;
    b->magic = MAGIC_USED;
    b->prev = b->next = 0;
    return (void *)((uint8_t *)b + sizeof(block_t));
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    if (b->magic != MAGIC_USED) return;
    b->magic = MAGIC_FREE;
    b->prev  = 0;
    b->next  = free_list;
    if (free_list) free_list->prev = b;
    free_list = b;
    coalesce(b);
}

uint32_t kmalloc_used(void) {
    uint32_t used = 0;
    uint32_t pos = heap_start;
    while (pos < heap_end) {
        block_t *b = (block_t *)pos;
        if (b->magic == MAGIC_USED) used += b->size;
        pos += sizeof(block_t) + b->size;
    }
    return used;
}

uint32_t kmalloc_free(void) {
    uint32_t fr = 0;
    for (block_t *b = free_list; b; b = b->next) fr += b->size;
    return fr;
}
