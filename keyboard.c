#include "kernel.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static char keyboard_buffer[256];
static volatile int buffer_index = 0;
static volatile int buffer_read = 0;

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
static int caps_lock = 0;

void keyboard_init(void) {
    buffer_index = 0;
    buffer_read = 0;
    shift_pressed = 0;
    caps_lock = 0;
}

void keyboard_handler(void) {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    
    if (status & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        
        // Handle shift keys (press)
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            return;
        }
        // Handle shift keys (release)
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
            return;
        }
        
        // Handle caps lock
        if (scancode == 0x3A) {
            caps_lock = !caps_lock;
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
        
        // Apply caps lock to letters
        if (caps_lock && c >= 'a' && c <= 'z' && !shift_pressed) {
            c = c - 'a' + 'A';
        } else if (caps_lock && c >= 'A' && c <= 'Z' && shift_pressed) {
            c = c - 'A' + 'a';
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
        // Small delay to prevent CPU spinning too fast
        for (volatile int i = 0; i < 100; i++);
    }
    
    char c = keyboard_buffer[buffer_read];
    buffer_read = (buffer_read + 1) % 256;
    return c;
}

int keyboard_available(void) {
    keyboard_handler();
    return buffer_read != buffer_index;
}

void keyboard_wait_for_key(void) {
    keyboard_getchar();
}
