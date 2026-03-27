#include "vga.h"
#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((uint16_t*)0xB8000)
#define VGA_PORT_CTRL 0x3D4
#define VGA_PORT_DATA 0x3D5

static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t current_color;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static uint8_t make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

static void update_hw_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(VGA_PORT_CTRL, 14);
    outb(VGA_PORT_DATA, (uint8_t)(pos >> 8));
    outb(VGA_PORT_CTRL, 15);
    outb(VGA_PORT_DATA, (uint8_t)(pos & 0xFF));
}

void vga_init(void) {
    current_color = make_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    current_color = make_color(fg, bg);
}

void vga_clear(void) {
    for (int r = 0; r < VGA_HEIGHT; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            VGA_MEMORY[r * VGA_WIDTH + c] = make_entry(' ', current_color);
    cursor_row = 0;
    cursor_col = 0;
    update_hw_cursor();
}

static void scroll(void) {
    /* Move all lines up one */
    for (int r = 0; r < VGA_HEIGHT - 1; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            VGA_MEMORY[r * VGA_WIDTH + c] = VGA_MEMORY[(r + 1) * VGA_WIDTH + c];
    /* Clear last line */
    for (int c = 0; c < VGA_WIDTH; c++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = make_entry(' ', current_color);
    cursor_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) scroll();
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= VGA_HEIGHT) scroll();
        }
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = make_entry(' ', current_color);
        }
    } else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = make_entry(c, current_color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= VGA_HEIGHT) scroll();
        }
    }
    update_hw_cursor();
}

void vga_puts(const char *str) {
    while (*str) vga_putchar(*str++);
}

void vga_set_cursor(int row, int col) {
    cursor_row = row;
    cursor_col = col;
    update_hw_cursor();
}

void vga_get_cursor(int *row, int *col) {
    *row = cursor_row;
    *col = cursor_col;
}

void vga_print_colored(const char *str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved = current_color;
    current_color = make_color(fg, bg);
    vga_puts(str);
    current_color = saved;
}

void vga_print_banner(void) {
    uint8_t saved = current_color;

    /* Full-width colored header bar */
    current_color = make_color(VGA_BLACK, VGA_CYAN);
    for (int c = 0; c < VGA_WIDTH; c++)
        VGA_MEMORY[0 * VGA_WIDTH + c] = make_entry(' ', current_color);

    /* Center the title */
    const char *title = "  KumOS v1.0  ";
    int len = 0;
    for (const char *p = title; *p; p++) len++;
    int start_col = (VGA_WIDTH - len) / 2;
    current_color = make_color(VGA_BLACK, VGA_CYAN);
    cursor_row = 0;
    cursor_col = start_col;
    vga_puts(title);

    cursor_row = 1;
    cursor_col = 0;
    current_color = saved;
}
