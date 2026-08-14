#include "vga.h"
#include <stdint.h>
#include <stddef.h>

static volatile uint16_t *const VGA_MEM = (uint16_t *)0xB8000;

static int terminal_row;
static int terminal_col;
static uint8_t terminal_color;

/* ANSI/VT100 CSI/OSC parser: colors (SGR incl. reverse video), cursor
   positioning/save-restore/show-hide, real erase-mode handling for J/K,
   scroll up/down, and private-mode (CSI ?) + OSC sequences consumed
   without leaking their bytes onscreen as garbage text. */
#define ANSI_MAX_PARAMS 8
static int     ansi_esc_state = 0;   /* 0=normal,1=saw ESC,2=in CSI,3=in OSC,4=OSC ST-pending */
static int     ansi_params[ANSI_MAX_PARAMS];
static int     ansi_nparams = 0;
static int     ansi_private = 0;     /* CSI ? ... seen */
static uint8_t ansi_fg = VGA_LIGHT_GREY;
static uint8_t ansi_bg = VGA_BLACK;
static int     ansi_bold = 0;
static int     ansi_reverse = 0;
static int     ansi_saved_row = 0;
static int     ansi_saved_col = 0;

static const uint8_t ansi_color_map[8] = {
    VGA_BLACK, VGA_RED, VGA_GREEN, VGA_BROWN,
    VGA_BLUE, VGA_MAGENTA, VGA_CYAN, VGA_LIGHT_GREY
};
static const uint8_t ansi_bright_color_map[8] = {
    VGA_DARK_GREY, VGA_LIGHT_RED, VGA_LIGHT_GREEN, VGA_YELLOW,
    VGA_LIGHT_BLUE, VGA_LIGHT_MAGENTA, VGA_LIGHT_CYAN, VGA_WHITE
};

static void ansi_apply_sgr(void) {
    if (ansi_nparams == 0 || (ansi_nparams == 1 && ansi_params[0] == 0)) {
        ansi_fg = VGA_LIGHT_GREY; ansi_bg = VGA_BLACK; ansi_bold = 0; ansi_reverse = 0;
        vga_set_color((vga_color)ansi_fg, (vga_color)ansi_bg);
        return;
    }
    for (int i = 0; i < ansi_nparams; i++) {
        int p = ansi_params[i];
        if (p == 0) { ansi_fg = VGA_LIGHT_GREY; ansi_bg = VGA_BLACK; ansi_bold = 0; ansi_reverse = 0; }
        else if (p == 1) ansi_bold = 1;
        else if (p == 22) ansi_bold = 0;
        else if (p == 7) ansi_reverse = 1;
        else if (p == 27) ansi_reverse = 0;
        /* 2=dim, 3=italic, 4=underline, 9=strikethrough: recognized and
           consumed but VGA text-mode colour cells have no way to render
           them, so they're intentionally no-ops rather than left unhandled. */
        else if (p >= 30 && p <= 37) ansi_fg = ansi_bold ? ansi_bright_color_map[p-30] : ansi_color_map[p-30];
        else if (p == 39) ansi_fg = VGA_LIGHT_GREY;
        else if (p >= 40 && p <= 47) ansi_bg = ansi_color_map[p-40];
        else if (p == 49) ansi_bg = VGA_BLACK;
        else if (p >= 90 && p <= 97) ansi_fg = ansi_bright_color_map[p-90];
        else if (p >= 100 && p <= 107) ansi_bg = ansi_bright_color_map[p-100];
    }
    if (ansi_reverse) vga_set_color((vga_color)ansi_bg, (vga_color)ansi_fg);
    else vga_set_color((vga_color)ansi_fg, (vga_color)ansi_bg);
}

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
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

static void vga_cursor_show(int show) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, show ? 0x0E : 0x20); /* bit5 = cursor disable */
}

/* Inclusive cell range fill, used by CSI J/K erase modes. */
static void vga_erase_range(int r1, int c1, int r2, int c2) {
    int start = r1 * VGA_WIDTH + c1;
    int end   = r2 * VGA_WIDTH + c2;
    for (int i = start; i <= end; i++)
        VGA_MEM[i] = vga_entry(' ', terminal_color);
}

static void vga_scroll_up_lines(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = VGA_MEM[(y+1) * VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT-1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
}

static void vga_scroll_down_lines(void) {
    for (int y = VGA_HEIGHT - 1; y > 0; y--)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = VGA_MEM[(y-1) * VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[x] = vga_entry(' ', terminal_color);
}

/* Alt screen buffer (CSI ?1049h/l, ?47h/l - what vi/ed-style full-screen
   programs use to leave the scrollback untouched and restore it on exit).
   The real VGA text-mode memory at 0xB8000 has no second hardware page to
   flip to like the framebuffer's double-buffering does, so this is a
   plain software copy into a kmalloc-free static array - 4000 bytes, cheap
   enough to always keep resident rather than allocate on first use. */
static uint16_t alt_screen_buf[VGA_WIDTH * VGA_HEIGHT];
static int      alt_screen_active = 0;
static int      alt_saved_row = 0, alt_saved_col = 0;

static void vga_enter_alt_screen(void) {
    if (alt_screen_active) return;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) alt_screen_buf[i] = VGA_MEM[i];
    alt_saved_row = terminal_row;
    alt_saved_col = terminal_col;
    alt_screen_active = 1;
    vga_clear();
}

static void vga_exit_alt_screen(void) {
    if (!alt_screen_active) return;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) VGA_MEM[i] = alt_screen_buf[i];
    alt_screen_active = 0;
    vga_goto(alt_saved_row, alt_saved_col);
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
    vga_scroll_up_lines();
    terminal_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (ansi_esc_state == 1) {
        if (c == '[') { ansi_esc_state = 2; ansi_nparams = 0; ansi_params[0] = 0; ansi_private = 0; }
        else if (c == ']') { ansi_esc_state = 3; }               /* OSC ... swallow */
        else if (c == '7') { ansi_saved_row = terminal_row; ansi_saved_col = terminal_col; ansi_esc_state = 0; }
        else if (c == '8') { vga_goto(ansi_saved_row, ansi_saved_col); ansi_esc_state = 0; }
        else ansi_esc_state = 0;
        return;
    }
    if (ansi_esc_state == 3) {                                    /* inside OSC, swallow to BEL or ESC \ */
        if ((uint8_t)c == 7) ansi_esc_state = 0;
        else if ((uint8_t)c == 27) ansi_esc_state = 4;
        return;
    }
    if (ansi_esc_state == 4) { ansi_esc_state = 0; return; }       /* the '\' terminating an ST */
    if (ansi_esc_state == 2) {
        if (c == '?') { ansi_private = 1; return; }
        if (c >= '0' && c <= '9') {
            if (ansi_nparams == 0) { ansi_nparams = 1; ansi_params[0] = 0; }
            ansi_params[ansi_nparams-1] = ansi_params[ansi_nparams-1]*10 + (c-'0');
        } else if (c == ';') {
            if (ansi_nparams < ANSI_MAX_PARAMS) { ansi_nparams++; ansi_params[ansi_nparams-1] = 0; }
        } else {
            if (ansi_nparams == 0) { ansi_nparams = 1; ansi_params[0] = 0; }
            switch (c) {
                case 'm': ansi_apply_sgr(); break;
                case 'H': case 'f': {
                    int row = ansi_params[0] ? ansi_params[0] : 1;
                    int col = (ansi_nparams > 1 && ansi_params[1]) ? ansi_params[1] : 1;
                    if (row > VGA_HEIGHT) row = VGA_HEIGHT;
                    if (col > VGA_WIDTH)  col = VGA_WIDTH;
                    vga_goto(row-1, col-1);
                    break;
                }
                case 'J': {
                    int mode = ansi_params[0];
                    if (mode == 1) vga_erase_range(0, 0, terminal_row, terminal_col);
                    else if (mode == 2 || mode == 3) vga_erase_range(0, 0, VGA_HEIGHT-1, VGA_WIDTH-1);
                    else vga_erase_range(terminal_row, terminal_col, VGA_HEIGHT-1, VGA_WIDTH-1);
                    break;
                }
                case 'K': {
                    int mode = ansi_params[0];
                    if (mode == 1) vga_erase_range(terminal_row, 0, terminal_row, terminal_col);
                    else if (mode == 2) vga_erase_range(terminal_row, 0, terminal_row, VGA_WIDTH-1);
                    else vga_erase_range(terminal_row, terminal_col, terminal_row, VGA_WIDTH-1);
                    break;
                }
                case 'A': { int n=ansi_params[0]?ansi_params[0]:1; int r=terminal_row-n; if(r<0)r=0; vga_goto(r, terminal_col); break; }
                case 'B': { int n=ansi_params[0]?ansi_params[0]:1; int r=terminal_row+n; if(r>=VGA_HEIGHT)r=VGA_HEIGHT-1; vga_goto(r, terminal_col); break; }
                case 'C': { int n=ansi_params[0]?ansi_params[0]:1; int cc=terminal_col+n; if(cc>=VGA_WIDTH)cc=VGA_WIDTH-1; vga_goto(terminal_row, cc); break; }
                case 'D': { int n=ansi_params[0]?ansi_params[0]:1; int cc=terminal_col-n; if(cc<0)cc=0; vga_goto(terminal_row, cc); break; }
                case 'E': { int n=ansi_params[0]?ansi_params[0]:1; int r=terminal_row+n; if(r>=VGA_HEIGHT)r=VGA_HEIGHT-1; vga_goto(r, 0); break; }
                case 'F': { int n=ansi_params[0]?ansi_params[0]:1; int r=terminal_row-n; if(r<0)r=0; vga_goto(r, 0); break; }
                case 'G': { int col=ansi_params[0]?ansi_params[0]-1:0; if(col<0)col=0; if(col>=VGA_WIDTH)col=VGA_WIDTH-1; vga_goto(terminal_row, col); break; }
                case 'S': { int n=ansi_params[0]?ansi_params[0]:1; for(int i=0;i<n;i++) vga_scroll_up_lines(); break; }
                case 'T': { int n=ansi_params[0]?ansi_params[0]:1; for(int i=0;i<n;i++) vga_scroll_down_lines(); break; }
                case 's': ansi_saved_row = terminal_row; ansi_saved_col = terminal_col; break;
                case 'u': vga_goto(ansi_saved_row, ansi_saved_col); break;
                case 'h':
                    if (ansi_private && ansi_params[0] == 25) vga_cursor_show(1);
                    else if (ansi_private && (ansi_params[0] == 1049 || ansi_params[0] == 47)) vga_enter_alt_screen();
                    break;
                case 'l':
                    if (ansi_private && ansi_params[0] == 25) vga_cursor_show(0);
                    else if (ansi_private && (ansi_params[0] == 1049 || ansi_params[0] == 47)) vga_exit_alt_screen();
                    break;
                default: break; /* unrecognized final byte: consumed, not printed - no garbage leak */
            }
            ansi_esc_state = 0;
            ansi_private = 0;
        }
        return;
    }
    if ((uint8_t)c == 27) { ansi_esc_state = 1; return; }

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

    VGA_MEM[y * VGA_WIDTH + x] = vga_entry((char)VGA_CH_TL, color);
    VGA_MEM[y * VGA_WIDTH + x + w - 1] = vga_entry((char)VGA_CH_TR, color);
    VGA_MEM[(y+h-1) * VGA_WIDTH + x] = vga_entry((char)VGA_CH_BL, color);
    VGA_MEM[(y+h-1) * VGA_WIDTH + x + w - 1] = vga_entry((char)VGA_CH_BR, color);

    for (int i = 1; i < w-1; i++) {
        VGA_MEM[y * VGA_WIDTH + x + i] = vga_entry((char)VGA_CH_HLINE, color);
        VGA_MEM[(y+h-1) * VGA_WIDTH + x + i] = vga_entry((char)VGA_CH_HLINE, color);
    }

    for (int i = 1; i < h-1; i++) {
        VGA_MEM[(y+i) * VGA_WIDTH + x] = vga_entry((char)VGA_CH_VLINE, color);
        VGA_MEM[(y+i) * VGA_WIDTH + x + w - 1] = vga_entry((char)VGA_CH_VLINE, color);
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

void vga_goto(int row, int col) {
    terminal_row = row;
    terminal_col = col;
    vga_set_cursor(col, row);
}

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