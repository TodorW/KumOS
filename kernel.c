#include "kernel.h"

// Kernel abstraction layer - allows for easy kernel swapping
static kernel_ops_t kumos_kernel_ops = {
    .init = NULL,
    .handle_interrupt = NULL,
    .schedule = NULL
};

static kernel_ops_t* current_kernel = &kumos_kernel_ops;

// Set kernel operations (for future Linux kernel integration)
void kernel_set_ops(kernel_ops_t* ops) {
    current_kernel = ops;
}

// Display welcome banner
void display_banner(void) {
    terminal_clear();
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("===============================================\n");
    terminal_writestring("              Welcome to KumOS                \n");
    terminal_writestring("         Custom Operating System v1.0         \n");
    terminal_writestring("===============================================\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    terminal_writestring("System Information:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("  Architecture: x86 (32-bit Protected Mode)\n");
    terminal_writestring("  Kernel: KumOS Custom Kernel\n");
    terminal_writestring("  Display: VGA Text Mode (80x25)\n");
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Type 'help' to see available commands\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

// Initialize all kernel subsystems
void kernel_init(void) {
    // Initialize terminal first (so we can print messages)
    terminal_initialize();
    
    // Display boot banner
    display_banner();
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("Initializing KumOS...\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n");
    
    // Initialize keyboard driver
    terminal_writestring("[  OK  ] Keyboard driver loaded\n");
    keyboard_init();
    
    // Initialize shell
    terminal_writestring("[  OK  ] Shell initialized\n");
    shell_init();
    
    terminal_writestring("[  OK  ] System ready\n");
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Boot complete! Welcome to KumOS.\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}

// Main kernel entry point (called from assembly)
void kernel_main(void) {
    // Initialize kernel subsystems
    kernel_init();
    
    // Call kernel init if available (for pluggable kernels)
    if (current_kernel && current_kernel->init) {
        current_kernel->init();
    }
    
    // Start the shell (this never returns)
    shell_run();
    
    // Should never reach here
    terminal_writestring("\n\nKernel panic: Shell exited unexpectedly!\n");
    while (1) {
        asm volatile ("hlt");
    }
}

// Kernel panic handler
void kernel_panic(const char* message) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    terminal_writestring("\n\n!!! KERNEL PANIC !!!\n");
    terminal_writestring(message);
    terminal_writestring("\n\nSystem halted.");
    
    asm volatile ("cli");
    while (1) {
        asm volatile ("hlt");
    }
}
