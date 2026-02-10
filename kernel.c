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
    terminal_writestring("  _  __                ___  ____\n");
    terminal_writestring(" | |/ /   _ _ __ ___  / _ \/ ___|n");
    terminal_writestring(" | ' / | | | '_ ` _ \| | | \___ \ n");
    terminal_writestring(" | . \ | |_| | | | | | |_| |___) |n");
    terminal_writestring(" |_|\ _\____,_|_| |_| \___/|____/ \n");
    terminal_writestring("                              \n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n");
    terminal_writestring("========================================\n");
    terminal_writestring("              KumOS v1.0                \n");
    terminal_writestring("========================================\n");
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    terminal_writestring("Welcome to KumOS!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Type 'help' to see available commands.\n");
    terminal_writestring("\n");
    
    terminal_writestring("Initializing system...\n");
}

// Initialize all kernel subsystems
void kernel_init(void) {
    // Initialize terminal first (so we can print messages)
    terminal_initialize();
    
    // Display boot banner
    display_banner();
    
    // Initialize keyboard driver
    terminal_writestring("Loading keyboard driver... ");
    keyboard_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    // Initialize shell
    terminal_writestring("Loading shell... ");
    shell_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("System initialized successfully!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
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