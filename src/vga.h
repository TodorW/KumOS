#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
} vga_color;

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color fg, vga_color bg);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_puts_at(const char *str, int x, int y, vga_color fg, vga_color bg);
void vga_set_cursor(int x, int y);
void vga_draw_box(int x, int y, int w, int h, vga_color fg, vga_color bg);
void vga_fill_rect(int x, int y, int w, int h, char ch, vga_color fg, vga_color bg);
int  vga_get_col(void);
int  vga_get_row(void);
void vga_scroll(void);
void vga_put_hex(uint32_t val);
void vga_put_dec(uint32_t val);

#endif