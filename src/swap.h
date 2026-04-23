#ifndef SWAP_H
#define SWAP_H
#include <stdint.h>

#define SWAP_SLOTS   256
#define SWAP_FILE    "SWAP.DAT"

int      swap_init(void);
int      swap_out(uint32_t virt_addr);
int      swap_in(uint32_t virt_addr);
int      swap_is_swapped(uint32_t virt_addr);
uint32_t swap_used_slots(void);

#endif
