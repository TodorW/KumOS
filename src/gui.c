
#include "gui.h"
#include "font8x8.h"
#include "vga.h"
#include "keyboard.h"
#include "mouse.h"
#include "timer.h"
#include "rtc.h"
#include "kstring.h"
#include "serial.h"
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }

static void set_mode13h(void) {

    outb(0x3C2, 0x63);

    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);

    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & 0x7F);

    static const uint8_t crtc13[25] = {
        0x5F,
        0x4F,
        0x50,
        0x82,
        0x54,
        0x80,
        0xBF,
        0x1F,
        0x00,
        0x41,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x9C,
        0x0E,
        0x8F,
        0x28,
        0x40,
        0x96,
        0xB9,
        0xA3,
        0xFF,
    };
    for (int i = 0; i < 25; i++) {
        outb(0x3D4, (uint8_t)i);
        outb(0x3D5, crtc13[i]);
    }

    static const uint8_t gc13[9] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x05,
        0x0F,
        0xFF,
    };
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, (uint8_t)i);
        outb(0x3CF, gc13[i]);
    }

    inb(0x3DA);
    static const uint8_t ac13[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41,
        0x00,
        0x0F,
        0x00,
        0x00,
    };
    for (int i = 0; i < 21; i++) {
        outb(0x3C0, (uint8_t)i);
        outb(0x3C0, ac13[i]);
    }
    outb(0x3C0, 0x20);

    while (  inb(0x3DA) & 0x08);
    while (!(inb(0x3DA) & 0x08));

    uint8_t *fb = (uint8_t *)0xA0000;
    for (int i = 0; i < 320*200; i++) fb[i] = 0;
}

static void set_mode3h(void) {
    outb(0x3C2, 0x67);
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x00);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & 0x7F);
    static const uint8_t crtc3[25] = {
        0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,0x00,
        0x4F,0x0D,0x0E,0x00,0x00,0x00,0x00,0x9C,0x0E,
        0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF
    };
    for (int i = 0; i < 25; i++) {
        outb(0x3D4, (uint8_t)i); outb(0x3D5, crtc3[i]);
    }
    static const uint8_t gc3[9] = {
        0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF
    };
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, (uint8_t)i); outb(0x3CF, gc3[i]);
    }
    inb(0x3DA);
    static const uint8_t ac3[21] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
        0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
        0x0C,0x00,0x0F,0x08,0x00
    };
    for (int i = 0; i < 21; i++) {
        outb(0x3C0, (uint8_t)i); outb(0x3C0, ac3[i]);
    }
    outb(0x3C0, 0x20);
}

static void set_palette(void) {

    struct { uint8_t r,g,b; } pal[32] = {
        {0,0,0},
        {0,0,42},
        {0,42,0},
        {0,42,42},
        {42,0,0},
        {42,0,42},
        {42,21,0},
        {42,42,42},
        {21,21,21},
        {21,21,63},
        {21,63,21},
        {21,63,63},
        {63,21,21},
        {63,21,63},
        {63,63,21},
        {63,63,63},
        {63,32,0},
        {0,0,32},
        {0,32,32},
        {32,63,0},
        {32,0,48},
        {0,0,20},
        {10,20,42},
        {16,28,50},
        {8,16,40},
        {52,52,56},
        {28,28,32},
        {4,4,4},
        {40,40,44},
        {50,50,55},
        {20,32,56},
        {63,63,0},
    };
    outb(0x3C8, 0);
    for (int i = 0; i < 32; i++) {
        outb(0x3C9, pal[i].r);
        outb(0x3C9, pal[i].g);
        outb(0x3C9, pal[i].b);
    }

    for (int i = 32; i < 256; i++) {
        uint8_t v = (uint8_t)(i / 4);
        outb(0x3C9, v); outb(0x3C9, v); outb(0x3C9, v);
    }
}

void gui_init(void) {

    set_mode13h();
    set_palette();
    mouse_set_bounds(GUI_WIDTH - 1, GUI_HEIGHT - 1);
}

void gui_exit(void) {
    set_mode3h();
    vga_init();
}

void gui_clear(uint8_t c) {
    uint8_t *fb = GUI_FB;
    for (int i = 0; i < GUI_WIDTH * GUI_HEIGHT; i++) fb[i] = c;
}

void gui_rect_fill(int x, int y, int w, int h, uint8_t c) {
    for (int row = y; row < y+h; row++)
        for (int col = x; col < x+w; col++)
            gui_pixel(col, row, c);
}

void gui_rect(int x, int y, int w, int h, uint8_t c) {
    gui_hline(x,   y,     w, c);
    gui_hline(x,   y+h-1, w, c);
    gui_vline(x,   y,     h, c);
    gui_vline(x+w-1, y,   h, c);
}

void gui_hline(int x, int y, int len, uint8_t c) {
    for (int i=0;i<len;i++) gui_pixel(x+i,y,c);
}
void gui_vline(int x, int y, int len, uint8_t c) {
    for (int i=0;i<len;i++) gui_pixel(x,y+i,c);
}
void gui_line(int x0,int y0,int x1,int y1,uint8_t c){
    int dx=x1-x0,dy=y1-y0,steps;
    if((dx<0?-dx:dx)>(dy<0?-dy:dy)) steps=dx<0?-dx:dx;
    else steps=dy<0?-dy:dy;
    if(!steps){gui_pixel(x0,y0,c);return;}

    int sx=(dx<<8)/steps, sy=(dy<<8)/steps;
    int cx=x0<<8, cy=y0<<8;
    for(int i=0;i<=steps;i++){
        gui_pixel(cx>>8,cy>>8,c); cx+=sx; cy+=sy;
    }
}

void gui_putchar(int x, int y, char c, uint8_t fg, uint8_t bg) {
    int idx = (unsigned char)c - 32;
    if (idx < 0 || idx >= 96) idx = 0;
    const uint8_t *glyph = font8x8[idx];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint8_t color = (bits & (0x80 >> col)) ? fg : bg;
            gui_pixel(x+col, y+row, color);
        }
    }
}

void gui_puts(int x, int y, const char *s, uint8_t fg, uint8_t bg) {
    int cx = x;
    while (*s) {
        if (*s == '\n') { cx = x; y += 9; s++; continue; }
        if (cx + 8 > GUI_WIDTH) { cx = x; y += 9; }
        gui_putchar(cx, y, *s++, fg, bg);
        cx += 8;
    }
}

void gui_printf(int x, int y, uint8_t fg, uint8_t bg, const char *fmt, ...) {
    char buf[128]; int pos=0;
    __builtin_va_list ap; __builtin_va_start(ap,fmt);
    while(*fmt && pos<126){
        if(*fmt!='%'){buf[pos++]=*fmt++;continue;}
        fmt++;
        if(*fmt=='s'){char*s=__builtin_va_arg(ap,char*);while(*s&&pos<126)buf[pos++]=*s++;}
        else if(*fmt=='d'){int d=__builtin_va_arg(ap,int);char t[12];int i=10;t[11]=0;if(!d){buf[pos++]='0';}else{if(d<0){buf[pos++]='-';d=-d;}unsigned u=(unsigned)d;while(u){t[i--]='0'+u%10;u/=10;}while(++i<=10)buf[pos++]=t[i];}
        }else if(*fmt=='u'){unsigned u=__builtin_va_arg(ap,unsigned);char t[12];int i=10;t[11]=0;if(!u){buf[pos++]='0';}else{while(u){t[i--]='0'+u%10;u/=10;}while(++i<=10)buf[pos++]=t[i];}}
        else if(*fmt=='x'){unsigned u=__builtin_va_arg(ap,unsigned);const char*h="0123456789ABCDEF";char t[12];int i=10;t[11]=0;if(!u){buf[pos++]='0';}else{while(u){t[i--]=h[u&0xF];u>>=4;}while(++i<=10)buf[pos++]=t[i];}}
        else if(*fmt=='c'){buf[pos++]=(char)__builtin_va_arg(ap,int);}
        else if(*fmt=='%'){buf[pos++]='%';}
        fmt++;
    }
    buf[pos]=0; __builtin_va_end(ap);
    gui_puts(x, y, buf, fg, bg);
}

void gui_window(int x, int y, int w, int h, const char *title) {

    gui_rect_fill(x+3, y+3, w, h, C_SHADOW);

    gui_rect_fill(x, y, w, h, C_WIN_BG);

    gui_rect_fill(x, y, w, 11, C_WIN_TITLE);

    gui_rect(x, y, w, h, C_WIN_BORDER);

    gui_puts(x+4, y+2, title, C_LIGHT_CYAN, C_WIN_TITLE);

    gui_rect_fill(x+w-11, y+1, 9, 9, C_RED);
    gui_puts(x+w-9, y+2, "X", C_WHITE, C_RED);
}

void gui_button(int x, int y, int w, int h, const char *label, int pressed) {
    uint8_t bg = pressed ? C_BUTTON : C_BUTTON_HI;
    gui_rect_fill(x, y, w, h, bg);
    gui_rect(x, y, w, h, C_WIN_BORDER);
    int tx = x + (w - (int)kstrlen(label)*8) / 2;
    int ty = y + (h - 8) / 2;
    gui_puts(tx, ty, label, C_WHITE, bg);
}

void gui_icon(int x, int y, const char *label, uint8_t icon_color) {

    gui_rect_fill(x, y, 32, 28, icon_color);
    gui_rect(x, y, 32, 28, C_LIGHT_GREY);

    int lx = x + (32 - (int)kstrlen(label)*8)/2;
    if (lx < x) lx = x;
    gui_puts(lx, y+30, label, C_WHITE, C_DESKTOP);
}

#define CUR_W 8
#define CUR_H 12
static uint8_t cursor_save[CUR_W * CUR_H];
static int     cursor_saved_x = -1, cursor_saved_y = -1;

static const uint8_t cursor_shape[CUR_H][CUR_W] = {
    {1,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0},
    {1,2,2,1,0,0,0,0},
    {1,2,2,2,1,0,0,0},
    {1,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,1,0},
    {1,2,2,2,1,1,1,0},
    {1,2,1,2,1,0,0,0},
    {1,1,0,1,2,1,0,0},
    {0,0,0,0,1,2,1,0},
    {0,0,0,0,0,1,1,0},
};

void gui_erase_cursor(int x, int y) {
    if (cursor_saved_x < 0) return;
    uint8_t *fb = GUI_FB;
    for (int row=0;row<CUR_H;row++)
        for (int col=0;col<CUR_W;col++) {
            int px=cursor_saved_x+col, py=cursor_saved_y+row;
            if ((unsigned)px<GUI_WIDTH&&(unsigned)py<GUI_HEIGHT)
                fb[py*GUI_WIDTH+px]=cursor_save[row*CUR_W+col];
        }
    cursor_saved_x=-1;
}

void gui_draw_cursor(int x, int y) {
    uint8_t *fb = GUI_FB;
    cursor_saved_x=x; cursor_saved_y=y;
    for (int row=0;row<CUR_H;row++)
        for (int col=0;col<CUR_W;col++) {
            int px=x+col, py=y+row;
            if ((unsigned)px<GUI_WIDTH&&(unsigned)py<GUI_HEIGHT) {
                cursor_save[row*CUR_W+col]=fb[py*GUI_WIDTH+px];
                uint8_t s=cursor_shape[row][col];
                if (s==1) fb[py*GUI_WIDTH+px]=C_WHITE;
                else if(s==2) fb[py*GUI_WIDTH+px]=C_BLACK;
            }
        }
}

void gui_draw_taskbar(void) {
    gui_rect_fill(0, 0, GUI_WIDTH, 10, C_TASKBAR);
    gui_hline(0, 9, GUI_WIDTH, C_WIN_BORDER);
    gui_puts(2, 1, "KumOS v1.5", C_LIGHT_CYAN, C_TASKBAR);

    rtc_time_t t = rtc_read();
    char timebuf[10];

    timebuf[0]='0'+t.hour/10;   timebuf[1]='0'+t.hour%10;
    timebuf[2]=':';
    timebuf[3]='0'+t.minute/10; timebuf[4]='0'+t.minute%10;
    timebuf[5]=':';
    timebuf[6]='0'+t.second/10; timebuf[7]='0'+t.second%10;
    timebuf[8]=0;
    gui_puts(GUI_WIDTH-67, 1, timebuf, C_YELLOW, C_TASKBAR);

    char ubuf[20];
    uint32_t up=timer_seconds();
    int ui=18; ubuf[19]=0;
    if(!up){ubuf[ui--]='0';}else{uint32_t u=up;while(u){ubuf[ui--]='0'+u%10;u/=10;}}
    ubuf[ui--]='s'; ubuf[ui--]='p'; ubuf[ui--]='u';
    gui_puts(GUI_WIDTH-130, 1, ubuf+ui+1, C_LIGHT_GREY, C_TASKBAR);
}

void gui_draw_desktop(void) {

    for (int y=10;y<GUI_HEIGHT;y++) {
        uint8_t c = (y < 120) ? C_DESKTOP : C_DARK_BLUE;
        gui_hline(0, y, GUI_WIDTH, c);
    }

    gui_icon(8,  18, "Term",  C_TEAL);
    gui_icon(8,  68, "Files", C_NAVY);
    gui_icon(8, 118, "Sys",   C_PURPLE);
    gui_icon(8, 168, "Exit",  C_DARK_GREY);

    gui_printf(52, 18, C_LIGHT_GREY, C_DESKTOP,
               "KumOS v1.5 - Press ESC to exit GUI");
    gui_printf(52, 28, C_DARK_GREY,  C_DESKTOP,
               "Mouse: click icons | Keyboard: terminal");
}

#define TERM_X    50
#define TERM_Y    12
#define TERM_W   270
#define TERM_H   120
#define TERM_ROWS  12
#define TERM_COLS  33
#define TERM_TX   (TERM_X+4)
#define TERM_TY   (TERM_Y+13)

static char  term_lines[TERM_ROWS][TERM_COLS+1];
static int   term_row = 0;
static char  term_input[TERM_COLS+1];
static int   term_icur = 0;

static void term_init(void) {
    for (int i=0;i<TERM_ROWS;i++) term_lines[i][0]=0;
    kstrcpy(term_lines[0], "KumOS v1.5 GUI terminal");
    kstrcpy(term_lines[1], "Type commands, Enter to run");
    kstrcpy(term_lines[2], "ESC = back to shell");
    term_row = 3;
    term_input[0]=0; term_icur=0;
}

static void term_scroll(void) {
    for (int i=0;i<TERM_ROWS-1;i++)
        kstrcpy(term_lines[i], term_lines[i+1]);
    term_lines[TERM_ROWS-1][0]=0;
    if (term_row > 0) term_row--;
}

static void term_puts(const char *s) {
    if (term_row >= TERM_ROWS) term_scroll();
    kstrcpy(term_lines[term_row], s);
    term_row++;
}

static void term_redraw(void) {

    gui_window(TERM_X, TERM_Y, TERM_W, TERM_H, "Terminal");

    gui_rect_fill(TERM_X+1, TERM_Y+12, TERM_W-2, TERM_H-22, C_BLACK);

    for (int i=0;i<TERM_ROWS-1;i++)
        if (term_lines[i][0])
            gui_puts(TERM_TX, TERM_TY + i*9, term_lines[i], C_LIGHT_GREEN, C_BLACK);

    int iy = TERM_TY + (TERM_ROWS-1)*9;
    gui_rect_fill(TERM_X+1, iy-1, TERM_W-2, 10, C_BLACK);
    gui_puts(TERM_TX, iy, "> ", C_YELLOW, C_BLACK);
    gui_puts(TERM_TX+16, iy, term_input, C_WHITE, C_BLACK);

    if ((timer_ticks() / 50) & 1)
        gui_rect_fill(TERM_TX+16+term_icur*8, iy, 7, 8, C_LIGHT_GREY);
}

static void term_exec(const char *cmd) {
    char line[TERM_COLS+1];

    if (kstrcmp(cmd,"help")==0) {
        term_puts("Commands: help date uptime");
        term_puts("          clear ls exit");
    } else if (kstrcmp(cmd,"date")==0) {
        rtc_time_t t = rtc_read();
        char buf[24];
        buf[0]='0'+t.year/1000; buf[1]='0'+(t.year/100)%10;
        buf[2]='0'+(t.year/10)%10; buf[3]='0'+t.year%10;
        buf[4]='-'; buf[5]='0'+t.month/10; buf[6]='0'+t.month%10;
        buf[7]='-'; buf[8]='0'+t.day/10;   buf[9]='0'+t.day%10;
        buf[10]=' ';buf[11]='0'+t.hour/10; buf[12]='0'+t.hour%10;
        buf[13]=':';buf[14]='0'+t.minute/10;buf[15]='0'+t.minute%10;
        buf[16]=':';buf[17]='0'+t.second/10;buf[18]='0'+t.second%10;
        buf[19]=0;
        term_puts(buf);
    } else if (kstrcmp(cmd,"uptime")==0) {
        uint32_t s=timer_seconds();
        line[0]=0;
        char num[12]; int i=10; num[11]=0;
        if(!s){num[i--]='0';}else{uint32_t u=s;while(u){num[i--]='0'+u%10;u/=10;}}
        kstrcpy(line,"Uptime: "); kstrcat(line,num+i+1); kstrcat(line,"s");
        term_puts(line);
    } else if (kstrcmp(cmd,"clear")==0) {
        for(int i=0;i<TERM_ROWS;i++) term_lines[i][0]=0;
        term_row=0;
    } else if (kstrcmp(cmd,"ls")==0) {
        term_puts("README.TXT  CONFIG.TXT");
        term_puts("HELLO.ELF   COUNTER.ELF");
    } else if (*cmd) {
        kstrcpy(line, "Unknown: "); kstrcat(line, cmd);
        term_puts(line);
    }
}

void gui_run(void) {
    gui_init();
    term_init();

    int prev_mx = -1, prev_my = -1;
    uint32_t last_taskbar = 0;
    int running = 1;
    int full_redraw = 1;

    while (running) {

        if (full_redraw) {
            gui_draw_desktop();
            term_redraw();
            full_redraw = 0;
        }

        uint32_t now = timer_seconds();
        if (now != last_taskbar) {
            gui_draw_taskbar();
            last_taskbar = now;
        }

        char key = keyboard_getchar();
        if (key) {
            if (key == 27) {
                running = 0;
            } else if (key == '\n') {

                char echo[TERM_COLS+3];
                kstrcpy(echo, "> "); kstrcat(echo, term_input);
                term_puts(echo);
                term_exec(term_input);
                term_input[0]=0; term_icur=0;
                full_redraw = 1;
            } else if (key == '\b') {
                if (term_icur > 0) { term_icur--; term_input[term_icur]=0; }
            } else if (term_icur < TERM_COLS-1) {
                term_input[term_icur++] = key;
                term_input[term_icur]   = 0;
            }
            term_redraw();
        }

        mouse_state_t *m = mouse_get();
        int mx = (int)m->x * GUI_WIDTH  / 80;
        int my = (int)m->y * GUI_HEIGHT / 25 + 10;
        if (my >= GUI_HEIGHT) my = GUI_HEIGHT-1;
        if (my < 0) my = 0;

        if (mx != prev_mx || my != prev_my) {
            gui_erase_cursor(prev_mx, prev_my);
            gui_draw_cursor(mx, my);
            prev_mx = mx; prev_my = my;
        }

        if (m->left && mx < 45) {
            int icon_y = my - 18;
            int icon_idx = icon_y / 50;
            if (icon_idx == 0 && icon_y >= 0) {

                full_redraw = 1;
            } else if (icon_idx == 3) {

                running = 0;
            }

            timer_sleep(200);
        }

        __asm__ volatile("hlt");
    }

    gui_erase_cursor(prev_mx, prev_my);
    gui_exit();
}