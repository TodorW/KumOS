#include "keyboard.h"
#include "io.h"
#include "screen.h"
#include "isr.h"

// Keyboard I/O port for data
#define KEYBOARD_DATA_PORT 0x60

// Keycode for the Enter and Backspace keys
#define KEY_ENTER 0x1C
#define KEY_BACKSPACE 0x0E

// Key map for US QWERTY keyboard (scancodes to ASCII)
unsigned char key_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', // Backspace
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',    // Enter
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',
    0,  // Alt
    ' ',// Space
    0,  // Caps lock
    0,  // F1 key ... > F10
    0, 0, 0, 0, 0, 0, 0, 0,
    0,  // F10
    0,  // Num lock
    0,  // Scroll Lock
    0,  // Home key
    0,  // Up Arrow
    0,  // Page Up
    '-',
    0,  // Left Arrow
    0,
    0,  // Right Arrow
    '+',
    0,  // End key
    0,  // Down Arrow
    0,  // Page Down
    0,  // Insert Key
    0,  // Delete Key
    0, 0, 0,
    0,  // F11 Key
    0,  // F12 Key
    0
};

// Function to handle keypress events
void keyboard_callback(registers_t *regs) {
    // Read scancode from keyboard data port
    unsigned char scancode = inb(KEYBOARD_DATA_PORT);

    // If the scancode is in the key_map range
    if (scancode < 128) {
        unsigned char key = key_map[scancode];
        
        if (key) {
            // Handle special keys
            if (key == '\b') {
                // Handle backspace
                screen_backspace();
            } else if (key == '\n') {
                // Handle Enter key
                screen_newline();
            } else {
                // Print regular character to screen
                screen_putc(key);
            }
        }
    }
}

// Initialize keyboard by setting the interrupt handler
void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback);
}
