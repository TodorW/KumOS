#include "vga.h"
#include <stdint.h>
#include <stddef.h>

static volatile uint16_t *const VGA_MEM = (uint16_t *)0xB8000;

static int terminal_row;
static int terminal_col;
static uint8_t terminal_color;

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static uint8_t vga_color_make(vga_color fg, vga_color bg) {
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void vga_set_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    terminal_row = 0;
    terminal_col = 0;
    terminal_color = vga_color_make(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    terminal_row = 0;
    terminal_col = 0;
    vga_set_cursor(0, 0);
}

void vga_set_color(vga_color fg, vga_color bg) {
    terminal_color = vga_color_make(fg, bg);
}

void vga_scroll(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = VGA_MEM[(y+1) * VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT-1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    terminal_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        terminal_col = 0;
        if (++terminal_row >= VGA_HEIGHT)
            vga_scroll();
    } else if (c == '\r') {
        terminal_col = 0;
    } else if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
            VGA_MEM[terminal_row * VGA_WIDTH + terminal_col] = vga_entry(' ', terminal_color);
        }
    } else if (c == '\t') {
        int spaces = 4 - (terminal_col % 4);
        for (int i = 0; i < spaces; i++) vga_putchar(' ');
        return;
    } else {
        VGA_MEM[terminal_row * VGA_WIDTH + terminal_col] = vga_entry(c, terminal_color);
        if (++terminal_col >= VGA_WIDTH) {
            terminal_col = 0;
            if (++terminal_row >= VGA_HEIGHT)
                vga_scroll();
        }
    }
    vga_set_cursor(terminal_col, terminal_row);
}

void vga_puts(const char *str) {
    while (*str) vga_putchar(*str++);
}

void vga_puts_at(const char *str, int x, int y, vga_color fg, vga_color bg) {
    uint8_t color = vga_color_make(fg, bg);
    int cx = x;
    while (*str && cx < VGA_WIDTH) {
        VGA_MEM[y * VGA_WIDTH + cx] = vga_entry(*str++, color);
        cx++;
    }
}

void vga_draw_box(int x, int y, int w, int h, vga_color fg, vga_color bg) {
    uint8_t color = vga_color_make(fg, bg);

    VGA_MEM[y * VGA_WIDTH + x] = vga_entry('+', color);
    VGA_MEM[y * VGA_WIDTH + x + w - 1] = vga_entry('+', color);
    VGA_MEM[(y+h-1) * VGA_WIDTH + x] = vga_entry('+', color);
    VGA_MEM[(y+h-1) * VGA_WIDTH + x + w - 1] = vga_entry('+', color);

    for (int i = 1; i < w-1; i++) {
        VGA_MEM[y * VGA_WIDTH + x + i] = vga_entry('-', color);
        VGA_MEM[(y+h-1) * VGA_WIDTH + x + i] = vga_entry('-', color);
    }

    for (int i = 1; i < h-1; i++) {
        VGA_MEM[(y+i) * VGA_WIDTH + x] = vga_entry('|', color);
        VGA_MEM[(y+i) * VGA_WIDTH + x + w - 1] = vga_entry('|', color);
    }
}

void vga_fill_rect(int x, int y, int w, int h, char ch, vga_color fg, vga_color bg) {
    uint8_t color = vga_color_make(fg, bg);
    for (int row = y; row < y+h && row < VGA_HEIGHT; row++)
        for (int col = x; col < x+w && col < VGA_WIDTH; col++)
            VGA_MEM[row * VGA_WIDTH + col] = vga_entry(ch, color);
}

int vga_get_col(void) { return terminal_col; }
int vga_get_row(void) { return terminal_row; }

void vga_put_hex(uint32_t val) {
    char buf[11];
    const char *hex = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[10] = 0;
    vga_puts(buf);
}

void vga_put_dec(uint32_t val) {
    if (val == 0) { vga_putchar('0'); return; }
    char buf[12]; int i = 10;
    buf[11] = 0;
    while (val > 0) { buf[i--] = '0' + (val % 10); val /= 10; }
    vga_puts(&buf[i+1]);
}