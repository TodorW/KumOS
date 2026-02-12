#ifndef KERNEL_H
#define KERNEL_H

// Define standard types manually for freestanding environment
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef unsigned long size_t;

#define NULL ((void*)0)

// Video memory and display
#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Colors
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW 14
#define VGA_COLOR_WHITE 15

// Keyboard scan codes
#define KEY_BACKSPACE 0x0E
#define KEY_ENTER 0x1C
#define KEY_ESCAPE 0x01

// Kernel abstraction layer (for future Linux kernel swap)
typedef struct {
    void (*init)(void);
    void (*handle_interrupt)(uint8_t irq);
    void (*schedule)(void);
} kernel_ops_t;

// Terminal functions
void terminal_initialize(void);
void terminal_clear(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_setcolor(uint8_t color);
void terminal_newline(void);
void terminal_backspace(void);
uint8_t vga_entry_color(uint8_t fg, uint8_t bg);

// String functions
size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);

// Keyboard functions
void keyboard_init(void);
char keyboard_getchar(void);
void keyboard_handler(void);
int keyboard_available(void);

// Shell functions
void shell_init(void);
void shell_run(void);
void shell_execute(char* command);

// Kernel functions
void display_banner(void);

// Utility functions
void itoa(int value, char* str, int base);
void reverse(char* str, int length);

// Port I/O
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif
