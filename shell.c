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
        terminal_writestring("\nKumOS v1.0 - Available Commands:\n");
        terminal_writestring("  help       - Show this help message\n");
        terminal_writestring("  clear      - Clear the screen\n");
        terminal_writestring("  echo       - Echo text to screen\n");
        terminal_writestring("  about      - Display system information\n");
        terminal_writestring("  uptime     - Show system uptime (placeholder)\n");
        terminal_writestring("  color      - Change text color (usage: color <0-15>)\n");
        terminal_writestring("  reboot     - Reboot the system\n");
        terminal_writestring("  shutdown   - Shutdown the system\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
        return; // Don't print newline
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
        terminal_writestring("KumOS v1.0\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("A simple custom operating system\n");
        terminal_writestring("Kernel: Custom KumOS Kernel\n");
        terminal_writestring("Architecture: x86 (32-bit)\n");
        terminal_writestring("License: Open Source\n");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        terminal_writestring("\nSystem uptime: Running since boot\n");
        terminal_writestring("(Timer not yet implemented)\n");
    }
    else if (strcmp(cmd, "color") == 0) {
        if (*arg) {
            int color = 0;
            // Simple atoi for single digit
            if (*arg >= '0' && *arg <= '9') {
                color = *arg - '0';
                if (*(arg + 1) >= '0' && *(arg + 1) <= '9') {
                    color = color * 10 + (*(arg + 1) - '0');
                }
            }
            if (color >= 0 && color <= 15) {
                terminal_setcolor(vga_entry_color(color, VGA_COLOR_BLACK));
                terminal_writestring("\nColor changed!\n");
            } else {
                terminal_writestring("\nInvalid color. Use 0-15\n");
            }
        } else {
            terminal_writestring("\nUsage: color <0-15>\n");
            terminal_writestring("Colors: 0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red,\n");
            terminal_writestring("        5=Magenta, 6=Brown, 7=LightGrey, 8=DarkGrey,\n");
            terminal_writestring("        9=LightBlue, 10=LightGreen, 11=LightCyan,\n");
            terminal_writestring("        12=LightRed, 13=LightMagenta, 14=Yellow, 15=White\n");
        }
    }
    else if (strcmp(cmd, "reboot") == 0) {
        terminal_writestring("\nRebooting system...\n");
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
        terminal_writestring("\nShutting down...\n");
        terminal_writestring("It is now safe to turn off your computer.\n");
        asm volatile ("cli; hlt");
    }
    else {
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Command not found: ");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring(cmd);
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
