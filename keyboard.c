#include "kernel.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static char keyboard_buffer[256];
static int buffer_index = 0;
static int buffer_read = 0;

// US QWERTY keyboard layout
static char keyboard_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static char keyboard_map_shifted[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int shift_pressed = 0;

void keyboard_init(void) {
    buffer_index = 0;
    buffer_read = 0;
    shift_pressed = 0;
}

void keyboard_handler(void) {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    
    if (status & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        
        // Handle shift keys
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            return;
        }
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
            return;
        }
        
        // Only handle key press (not release)
        if (scancode & 0x80) {
            return;
        }
        
        char c;
        if (shift_pressed) {
            c = keyboard_map_shifted[scancode];
        } else {
            c = keyboard_map[scancode];
        }
        
        if (c) {
            keyboard_buffer[buffer_index] = c;
            buffer_index = (buffer_index + 1) % 256;
        }
    }
}

char keyboard_getchar(void) {
    while (buffer_read == buffer_index) {
        keyboard_handler();
    }
    
    char c = keyboard_buffer[buffer_read];
    buffer_read = (buffer_read + 1) % 256;
    return c;
}

void keyboard_wait_for_key(void) {
    keyboard_getchar();
}
