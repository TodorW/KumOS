#include "vga.h"
#include "keyboard.h"
#include "klibc.h"
#include "memory.h"
#include "shell.h"
#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, void *mbi) {
    /* Init subsystems */
    vga_init();
    vga_print_banner();

    keyboard_init();
    pmm_init((multiboot_info_t *)mbi);

    if (magic != MULTIBOOT_MAGIC) {
        vga_print_colored("ERROR: Not loaded by a Multiboot-compliant bootloader!\n",
                          VGA_LIGHT_RED, VGA_BLACK);
        /* keep going anyway — QEMU direct kernel load */
    }

    /* Hand off to the shell */
    shell_run();

    /* Should never return */
    __asm__ volatile ("cli; hlt");
}
