#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define GUI_WIDTH    320
#define GUI_HEIGHT   200
#define GUI_FB       ((uint8_t *)0xA0000)

#define C_BLACK        0
#define C_BLUE         1
#define C_GREEN        2
#define C_CYAN         3
#define C_RED          4
#define C_MAGENTA      5
#define C_BROWN        6
#define C_LIGHT_GREY   7
#define C_DARK_GREY    8
#define C_LIGHT_BLUE   9
#define C_LIGHT_GREEN  10
#define C_LIGHT_CYAN   11
#define C_LIGHT_RED    12
#define C_PINK         13
#define C_YELLOW       14
#define C_WHITE        15

#define C_ORANGE       16
#define C_NAVY         17
#define C_TEAL         18
#define C_LIME         19
#define C_PURPLE       20
#define C_DARK_BLUE    21
#define C_TASKBAR      22
#define C_DESKTOP      23
#define C_WIN_TITLE    24
#define C_WIN_BG       25
#define C_WIN_BORDER   26
#define C_SHADOW       27
#define C_BUTTON       28
#define C_BUTTON_HI    29
#define C_ICON_BG      30
#define C_CURSOR       31

void gui_init(void);
void gui_exit(void);

static inline void gui_pixel(int x, int y, uint8_t c) {
    if ((unsigned)x < GUI_WIDTH && (unsigned)y < GUI_HEIGHT)
        GUI_FB[y * GUI_WIDTH + x] = c;
}
void gui_clear(uint8_t color);
void gui_rect(int x, int y, int w, int h, uint8_t color);
void gui_rect_fill(int x, int y, int w, int h, uint8_t color);
void gui_hline(int x, int y, int len, uint8_t color);
void gui_vline(int x, int y, int len, uint8_t color);
void gui_line(int x0, int y0, int x1, int y1, uint8_t color);

void gui_putchar(int x, int y, char c, uint8_t fg, uint8_t bg);
void gui_puts(int x, int y, const char *s, uint8_t fg, uint8_t bg);
void gui_printf(int x, int y, uint8_t fg, uint8_t bg, const char *fmt, ...);

void gui_window(int x, int y, int w, int h, const char *title);
void gui_button(int x, int y, int w, int h, const char *label, int pressed);
void gui_icon(int x, int y, const char *label, uint8_t icon_color);

void gui_draw_cursor(int x, int y);
void gui_erase_cursor(int x, int y);

void gui_draw_desktop(void);
void gui_draw_taskbar(void);

void gui_run(void);

#endif