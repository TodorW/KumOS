#include "swap.h"
#include "paging.h"
#include "fat12.h"
#include "kstring.h"
#include "serial.h"
#include <stdint.h>

#define PAGE_SIZE 4096

typedef struct {
    uint32_t virt_addr;
    uint32_t slot;
    int      used;
} swap_entry_t;

static swap_entry_t swap_table[SWAP_SLOTS];
static int          swap_ready   = 0;
static uint32_t     swap_used    = 0;

static uint8_t swap_buf[SWAP_SLOTS * PAGE_SIZE];

int swap_init(void) {
    if (!fat12_mounted()) return -1;
    kmemset(swap_table, 0, sizeof(swap_table));
    kmemset(swap_buf, 0, sizeof(swap_buf));
    swap_used  = 0;
    swap_ready = 1;
    serial_printf("[swap] ready: %u slots x 4KB = %uKB\r\n",
                  SWAP_SLOTS, SWAP_SLOTS * 4);
    return 0;
}

static int alloc_slot(void) {
    for (int i = 0; i < SWAP_SLOTS; i++)
        if (!swap_table[i].used) return i;
    return -1;
}

int swap_out(uint32_t virt_addr) {
    if (!swap_ready) return -1;
    virt_addr &= ~0xFFF;

    for (int i = 0; i < SWAP_SLOTS; i++)
        if (swap_table[i].used && swap_table[i].virt_addr == virt_addr)
            return 0;

    int slot = alloc_slot();
    if (slot < 0) return -1;

    kmemcpy(swap_buf + slot * PAGE_SIZE, (void *)virt_addr, PAGE_SIZE);

    paging_unmap(virt_addr);
    pmm_free(paging_virt_to_phys(virt_addr));

    swap_table[slot].virt_addr = virt_addr;
    swap_table[slot].slot      = (uint32_t)slot;
    swap_table[slot].used      = 1;
    swap_used++;

    serial_printf("[swap] paged out 0x%x -> slot %d\r\n", virt_addr, slot);
    return 0;
}

int swap_in(uint32_t virt_addr) {
    if (!swap_ready) return -1;
    virt_addr &= ~0xFFF;

    for (int i = 0; i < SWAP_SLOTS; i++) {
        if (!swap_table[i].used) continue;
        if (swap_table[i].virt_addr != virt_addr) continue;

        uint32_t phys = pmm_alloc();
        if (!phys) return -1;

        kmemcpy((void *)phys, swap_buf + i * PAGE_SIZE, PAGE_SIZE);
        paging_map(virt_addr, phys, PAGE_WRITE | PAGE_USER);

        swap_table[i].used = 0;
        swap_used--;

        serial_printf("[swap] paged in 0x%x from slot %d\r\n", virt_addr, i);
        return 0;
    }
    return -1;
}

int swap_is_swapped(uint32_t virt_addr) {
    virt_addr &= ~0xFFF;
    for (int i = 0; i < SWAP_SLOTS; i++)
        if (swap_table[i].used && swap_table[i].virt_addr == virt_addr)
            return 1;
    return 0;
}

uint32_t swap_used_slots(void) { return swap_used; }
