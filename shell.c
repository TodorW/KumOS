#include "kernel.h"

#define MAX_COMMAND_LENGTH 256

static char command_buffer[MAX_COMMAND_LENGTH];
static int command_index = 0;

// Command history
#define HISTORY_SIZE 10
static char command_history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_index = 0;
static int history_count = 0;

void shell_print_prompt(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("KumOS");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring(":");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    terminal_writestring("~");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("$ ");
}

void shell_init(void) {
    command_index = 0;
    history_index = 0;
    history_count = 0;
    memset(command_buffer, 0, MAX_COMMAND_LENGTH);
}

void shell_execute(char* command) {
    // Trim leading spaces
    while (*command == ' ') command++;
    
    // Empty command
    if (*command == '\0') {
        return;
    }
    
    // Parse command and arguments
    char* cmd = command;
    char* arg = command;
    
    // Find first space
    while (*arg && *arg != ' ') arg++;
    if (*arg) {
        *arg = '\0';
        arg++;
        while (*arg == ' ') arg++; // Skip spaces
    }
    
    // Built-in commands
    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("KumOS v1.0 - Available Commands\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("=================================\n\n");
        terminal_writestring("System Information:\n");
        terminal_writestring("  help       - Show this help message\n");
        terminal_writestring("  about      - Display detailed system information\n");
        terminal_writestring("  ver        - Show OS version\n");
        terminal_writestring("  sysinfo    - Show hardware information\n");
        terminal_writestring("  uptime     - Show system uptime\n");
        terminal_writestring("\nDisplay Commands:\n");
        terminal_writestring("  clear      - Clear the screen\n");
        terminal_writestring("  cls        - Alias for clear\n");
        terminal_writestring("  color      - Change text color (usage: color <0-15>)\n");
        terminal_writestring("  colors     - Show all available colors\n");
        terminal_writestring("\nUtility Commands:\n");
        terminal_writestring("  echo       - Echo text to screen\n");
        terminal_writestring("  banner     - Show boot banner\n");
        terminal_writestring("  date       - Show current date (placeholder)\n");
        terminal_writestring("  time       - Show current time (placeholder)\n");
        terminal_writestring("\nSystem Control:\n");
        terminal_writestring("  reboot     - Reboot the system\n");
        terminal_writestring("  shutdown   - Shutdown the system\n");
        terminal_writestring("  halt       - Halt the CPU\n");
    }
    else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        terminal_clear();
        return;
    }
    else if (strcmp(cmd, "echo") == 0) {
        terminal_writestring("\n");
        if (*arg) {
            terminal_writestring(arg);
        }
    }
    else if (strcmp(cmd, "about") == 0) {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("========================================\n");
        terminal_writestring("            KumOS v1.0                  \n");
        terminal_writestring("========================================\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("\nA custom operating system built from scratch\n");
        terminal_writestring("\nSystem Details:\n");
        terminal_writestring("  Kernel: Custom KumOS Kernel\n");
        terminal_writestring("  Architecture: x86 (32-bit)\n");
        terminal_writestring("  Boot Mode: Protected Mode\n");
        terminal_writestring("  Display: VGA Text Mode\n");
        terminal_writestring("  Resolution: 80x25 characters\n");
        terminal_writestring("  Colors: 16 foreground, 8 background\n");
        terminal_writestring("\nFeatures:\n");
        terminal_writestring("  - Custom bootloader\n");
        terminal_writestring("  - VGA terminal driver\n");
        terminal_writestring("  - PS/2 keyboard support\n");
        terminal_writestring("  - Interactive shell\n");
        terminal_writestring("  - Modular kernel design\n");
        terminal_writestring("\nLicense: Open Source\n");
    }
    else if (strcmp(cmd, "ver") == 0 || strcmp(cmd, "version") == 0) {
        terminal_writestring("\nKumOS Version 1.0.0\n");
        terminal_writestring("Built: 2025\n");
        terminal_writestring("Kernel: Custom x86 Kernel\n");
    }
    else if (strcmp(cmd, "sysinfo") == 0) {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        terminal_writestring("Hardware Information\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("====================\n");
        terminal_writestring("CPU: x86 compatible processor\n");
        terminal_writestring("Mode: 32-bit Protected Mode\n");
        terminal_writestring("Memory: Available (no manager yet)\n");
        terminal_writestring("Display: VGA Text Mode (0xB8000)\n");
        terminal_writestring("Keyboard: PS/2 Controller\n");
        terminal_writestring("Boot Device: Floppy/USB\n");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        terminal_writestring("\nSystem uptime: Running since boot\n");
        terminal_writestring("(PIT timer not yet implemented)\n");
    }
    else if (strcmp(cmd, "date") == 0) {
        terminal_writestring("\nCurrent date: 2025-02-12\n");
        terminal_writestring("(RTC not yet implemented)\n");
    }
    else if (strcmp(cmd, "time") == 0) {
        terminal_writestring("\nCurrent time: --:--:--\n");
        terminal_writestring("(RTC not yet implemented)\n");
    }
    else if (strcmp(cmd, "banner") == 0) {
        terminal_writestring("\n");
        display_banner();
        return;
    }
    else if (strcmp(cmd, "colors") == 0) {
        terminal_writestring("\nAvailable Colors:\n");
        terminal_writestring("=================\n");
        for (int i = 0; i < 16; i++) {
            terminal_setcolor(vga_entry_color(i, VGA_COLOR_BLACK));
            char buf[3];
            if (i < 10) {
                buf[0] = '0' + i;
                buf[1] = '\0';
            } else {
                buf[0] = '1';
                buf[1] = '0' + (i - 10);
                buf[2] = '\0';
            }
            terminal_writestring(buf);
            terminal_writestring(" - ");
            
            // Color names
            const char* names[] = {
                "Black", "Blue", "Green", "Cyan", 
                "Red", "Magenta", "Brown", "Light Grey",
                "Dark Grey", "Light Blue", "Light Green", "Light Cyan",
                "Light Red", "Light Magenta", "Yellow", "White"
            };
            terminal_writestring(names[i]);
            terminal_writestring("\n");
        }
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("\nUsage: color <number>\n");
    }
    else if (strcmp(cmd, "color") == 0) {
        if (*arg) {
            int color = 0;
            // Simple atoi for single/double digit
            if (*arg >= '0' && *arg <= '9') {
                color = *arg - '0';
                if (*(arg + 1) >= '0' && *(arg + 1) <= '9') {
                    color = color * 10 + (*(arg + 1) - '0');
                }
            }
            if (color >= 0 && color <= 15) {
                terminal_setcolor(vga_entry_color(color, VGA_COLOR_BLACK));
                terminal_writestring("\nColor changed to ");
                char buf[3];
                itoa(color, buf, 10);
                terminal_writestring(buf);
                terminal_writestring("!\n");
            } else {
                terminal_writestring("\nInvalid color. Use 0-15\n");
                terminal_writestring("Type 'colors' to see all available colors.\n");
            }
        } else {
            terminal_writestring("\nUsage: color <0-15>\n");
            terminal_writestring("Type 'colors' to see all available colors.\n");
        }
    }
    else if (strcmp(cmd, "reboot") == 0) {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        terminal_writestring("Rebooting system...\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        
        // Small delay
        for (volatile int i = 0; i < 10000000; i++);
        
        // Use keyboard controller to reboot
        uint8_t temp;
        asm volatile ("cli");
        do {
            temp = inb(0x64);
            if (temp & 1)
                inb(0x60);
        } while (temp & 2);
        outb(0x64, 0xFE);
        asm volatile ("hlt");
    }
    else if (strcmp(cmd, "shutdown") == 0) {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        terminal_writestring("Shutting down...\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("It is now safe to turn off your computer.\n");
        terminal_writestring("\nPress any key to halt...\n");
        keyboard_getchar();
        asm volatile ("cli; hlt");
    }
    else if (strcmp(cmd, "halt") == 0) {
        terminal_writestring("\nHalting CPU...\n");
        asm volatile ("cli; hlt");
    }
    else {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Command not found: ");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring(cmd);
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("\nType 'help' for available commands.\n");
    }
    
    terminal_writestring("\n");
}

void shell_run(void) {
    terminal_writestring("\n");
    shell_print_prompt();
    
    while (1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            command_buffer[command_index] = '\0';
            terminal_putchar('\n');
            
            // Add to history if not empty
            if (command_index > 0) {
                strcpy(command_history[history_count % HISTORY_SIZE], command_buffer);
                history_count++;
                shell_execute(command_buffer);
            }
            
            command_index = 0;
            memset(command_buffer, 0, MAX_COMMAND_LENGTH);
            shell_print_prompt();
        }
        else if (c == '\b') {
            if (command_index > 0) {
                command_index--;
                command_buffer[command_index] = '\0';
                terminal_backspace();
            }
        }
        else if (c >= 32 && c <= 126) { // Printable characters
            if (command_index < MAX_COMMAND_LENGTH - 1) {
                command_buffer[command_index++] = c;
                terminal_putchar(c);
            }
        }
    }
}
