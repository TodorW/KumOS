
#include "gui.h"
#include "font8x8.h"
#include "vga.h"
#include "keyboard.h"
#include "mouse.h"
#include "timer.h"
#include "rtc.h"
#include "kstring.h"
#include "fat12.h"
#include "kmalloc.h"
#include "paging.h"
#include "sched.h"
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
    mouse_set_bounds(79, 24);
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
            uint8_t color = (bits & (0x01 << col)) ? fg : bg;
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

void gui_draw_cursor(int x, int y) {
    uint8_t *fb = GUI_FB;
    for (int row=0;row<CUR_H;row++)
        for (int col=0;col<CUR_W;col++) {
            int px=x+col, py=y+row;
            if ((unsigned)px<GUI_WIDTH&&(unsigned)py<GUI_HEIGHT) {
                uint8_t s=cursor_shape[row][col];
                if (s==1) fb[py*GUI_WIDTH+px]=C_WHITE;
                else if(s==2) fb[py*GUI_WIDTH+px]=C_BLACK;
            }
        }
}

static int point_in(int px, int py, int x, int y, int w, int h) {
    return px>=x && px<x+w && py>=y && py<y+h;
}

typedef enum { WIN_TERMINAL=0, WIN_FILES=1, WIN_SYSMON=2, WIN_COUNT=3 } wintype_t;

typedef struct { int active, x, y, w, h; } winrec_t;

static winrec_t wins[WIN_COUNT];
static int      zorder[WIN_COUNT];
static int      zcount = 0;

static const char *win_title(int t) {
    switch (t) {
        case WIN_TERMINAL: return "Terminal";
        case WIN_FILES:    return "Files";
        case WIN_SYSMON:   return "System Monitor";
    }
    return "";
}

static void wm_remove_z(int t) {
    int idx = -1;
    for (int i=0;i<zcount;i++) if (zorder[i]==t) { idx=i; break; }
    if (idx < 0) return;
    for (int i=idx;i<zcount-1;i++) zorder[i]=zorder[i+1];
    zcount--;
}

static void wm_push_front(int t) {
    wm_remove_z(t);
    zorder[zcount++] = t;
}

static int wm_topmost(void) { return zcount ? zorder[zcount-1] : -1; }

static void files_refresh(void);

static void wm_open(int t, int defx, int defy, int defw, int defh) {
    if (!wins[t].active) {
        wins[t].active = 1;
        wins[t].x = defx; wins[t].y = defy; wins[t].w = defw; wins[t].h = defh;
        if (t == WIN_FILES) files_refresh();
    }
    wm_push_front(t);
}

static void wm_close(int t) {
    wins[t].active = 0;
    wm_remove_z(t);
}

#define TB_TAB_MAX WIN_COUNT
static int tb_tab_type[TB_TAB_MAX];
static int tb_tab_x0[TB_TAB_MAX];
static int tb_tab_x1[TB_TAB_MAX];
static int tb_tab_count = 0;

void gui_draw_taskbar(void) {
    gui_rect_fill(0, 0, GUI_WIDTH, 10, C_TASKBAR);
    gui_hline(0, 9, GUI_WIDTH, C_WIN_BORDER);
    gui_puts(2, 1, "KumOS", C_LIGHT_CYAN, C_TASKBAR);

    tb_tab_count = 0;
    int tx = 36;
    int top = wm_topmost();
    for (int i=0;i<zcount;i++) {
        int t = zorder[i];
        if (!wins[t].active) continue;
        int tw = (int)kstrlen(win_title(t))*8 + 6;
        uint8_t bg = (t==top) ? C_BUTTON_HI : C_TASKBAR;
        gui_rect_fill(tx, 0, tw, 9, bg);
        gui_puts(tx+3, 1, win_title(t), C_WHITE, bg);
        tb_tab_type[tb_tab_count]=t; tb_tab_x0[tb_tab_count]=tx; tb_tab_x1[tb_tab_count]=tx+tw;
        tb_tab_count++;
        tx += tw + 2;
    }

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
    int ui=17; ubuf[18]='s'; ubuf[19]=0;
    if(!up){ubuf[ui--]='0';}else{uint32_t u=up;while(u){ubuf[ui--]='0'+u%10;u/=10;}}
    ubuf[ui--]='p'; ubuf[ui--]='u';
    gui_puts(GUI_WIDTH-130, 1, ubuf+ui+1, C_LIGHT_GREY, C_TASKBAR);
}

static int taskbar_hit(int mx, int my) {
    if (my >= 10) return -1;
    for (int i=0;i<tb_tab_count;i++)
        if (mx>=tb_tab_x0[i] && mx<tb_tab_x1[i]) return tb_tab_type[i];
    return -2;
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
               "Click icons to open apps");
    gui_printf(52, 28, C_DARK_GREY,  C_DESKTOP,
               "Drag bar to move, Esc to close");
}

#define TERM_W    270
#define TERM_H    120
#define TERM_ROWS  12
#define TERM_COLS  33

static char  term_lines[TERM_ROWS][TERM_COLS+1];
static int   term_row = 0;
static char  term_input[TERM_COLS+1];
static int   term_icur = 0;

static void term_init(void) {
    for (int i=0;i<TERM_ROWS;i++) term_lines[i][0]=0;
    kstrcpy(term_lines[0], "KumOS GUI terminal");
    kstrcpy(term_lines[1], "help for commands, exit to close");
    term_row = 2;
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

static void render_terminal(winrec_t *w) {

    gui_window(w->x, w->y, w->w, w->h, "Terminal");

    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-22, C_BLACK);

    int tx = w->x+4, ty = w->y+13;
    for (int i=0;i<TERM_ROWS-1;i++)
        if (term_lines[i][0])
            gui_puts(tx, ty + i*9, term_lines[i], C_LIGHT_GREEN, C_BLACK);

    int iy = ty + (TERM_ROWS-1)*9;
    gui_rect_fill(w->x+1, iy-1, w->w-2, 10, C_BLACK);
    gui_puts(tx, iy, "> ", C_YELLOW, C_BLACK);
    gui_puts(tx+16, iy, term_input, C_WHITE, C_BLACK);

    if ((timer_ticks() / 50) & 1)
        gui_rect_fill(tx+16+term_icur*8, iy, 7, 8, C_LIGHT_GREY);
}

static void term_exec(const char *cmd) {
    char line[TERM_COLS+1];
    char num[12];

    if (kstrcmp(cmd,"help")==0) {
        term_puts("help date uptime clear ls");
        term_puts("meminfo ps cat <f> exit");
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
        int i=10; num[11]=0;
        if(!s){num[i--]='0';}else{uint32_t u=s;while(u){num[i--]='0'+u%10;u/=10;}}
        kstrcpy(line,"Uptime: "); kstrcat(line,num+i+1); kstrcat(line,"s");
        term_puts(line);
    } else if (kstrcmp(cmd,"clear")==0) {
        for(int i=0;i<TERM_ROWS;i++) term_lines[i][0]=0;
        term_row=0;
    } else if (kstrcmp(cmd,"ls")==0) {
        if (!fat12_mounted()) {
            term_puts("No disk mounted");
        } else {
            fat12_entry_t ents[16];
            int n = fat12_list(ents, 16);
            if (n <= 0) term_puts("(empty)");
            for (int i=0;i<n;i++) term_puts(ents[i].name);
        }
    } else if (kstrcmp(cmd,"meminfo")==0) {
        kstrcpy(line,"heap "); kitoa(kmalloc_used()/1024,num,10); kstrcat(line,num);
        kstrcat(line,"K/"); kitoa(kmalloc_free()/1024,num,10); kstrcat(line,num);
        kstrcat(line,"K free");
        term_puts(line);
        kstrcpy(line,"frames "); kitoa(pmm_used(),num,10); kstrcat(line,num);
        kstrcat(line,"/"); kitoa(pmm_total(),num,10); kstrcat(line,num);
        term_puts(line);
    } else if (kstrcmp(cmd,"ps")==0) {
        int n = sched_task_count();
        for (int i=0;i<n;i++) {
            task_t *t = sched_task_at(i);
            if (!t) continue;
            kstrcpy(line,""); kitoa((uint32_t)t->pid,num,10);
            kstrcat(line,num); kstrcat(line," "); kstrcat(line,t->name);
            term_puts(line);
        }
    } else if (kstartswith(cmd,"cat ")) {
        if (!fat12_mounted()) {
            term_puts("No disk mounted");
        } else {
            uint8_t buf[128];
            int n = fat12_read(cmd+4, buf, sizeof(buf)-1);
            if (n < 0) {
                kstrcpy(line,"not found: "); kstrcat(line,cmd+4); term_puts(line);
            } else {
                buf[n]=0;
                char *p=(char*)buf; char row[TERM_COLS+1]; int ri=0;
                while (*p) {
                    if (*p=='\n' || ri==TERM_COLS) {
                        row[ri]=0; term_puts(row); ri=0;
                        if (*p=='\n') p++;
                        continue;
                    }
                    row[ri++]=*p++;
                }
                if (ri) { row[ri]=0; term_puts(row); }
            }
        }
    } else if (kstrcmp(cmd,"exit")==0) {
        wm_close(WIN_TERMINAL);
    } else if (*cmd) {
        kstrcpy(line, "Unknown: "); kstrcat(line, cmd);
        term_puts(line);
    }
}

#define FILES_MAX    32
#define FILES_ROW_H   9

static fat12_entry_t files_entries[FILES_MAX];
static int  files_count = 0;
static int  files_selected = -1;
static int  files_preview = 0;
static char files_previewbuf[256];

static void files_refresh(void) {
    files_count = fat12_mounted() ? fat12_list(files_entries, FILES_MAX) : 0;
    if (files_count < 0) files_count = 0;
    files_selected = -1;
    files_preview = 0;
}

static void render_files(winrec_t *w) {

    gui_window(w->x, w->y, w->w, w->h, "Files");
    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-13, C_BLACK);

    int cx = w->x+3, cy = w->y+13;

    if (!files_preview) {
        if (!fat12_mounted()) {
            gui_puts(cx, cy, "No disk mounted", C_LIGHT_RED, C_BLACK);
        } else if (files_count <= 0) {
            gui_puts(cx, cy, "(empty)", C_LIGHT_GREY, C_BLACK);
        } else {
            int maxrows = (w->h - 13 - 10) / FILES_ROW_H;
            for (int i=0;i<files_count && i<maxrows;i++) {
                int ry = cy + i*FILES_ROW_H - 1;
                uint8_t fg = (i==files_selected) ? C_BLACK : C_LIGHT_GREEN;
                uint8_t bg = (i==files_selected) ? C_LIGHT_GREEN : C_BLACK;
                if (i==files_selected) gui_rect_fill(w->x+1, ry, w->w-2, FILES_ROW_H, bg);
                gui_puts(cx, cy+i*FILES_ROW_H, files_entries[i].name, fg, bg);
            }
            gui_puts(cx, w->y+w->h-9, "Click a file to view", C_DARK_GREY, C_BLACK);
        }
    } else {
        gui_printf(cx, cy, C_YELLOW, C_BLACK, "%s", files_entries[files_selected].name);
        gui_puts(cx, cy+10, files_previewbuf, C_LIGHT_GREEN, C_BLACK);
        gui_button(w->x+w->w-40, w->y+w->h-14, 34, 11, "Back", 0);
    }
}

static void files_handle_click(winrec_t *w, int mx, int my) {
    int cy = w->y+13;

    if (!files_preview) {
        if (files_count <= 0) return;
        int maxrows = (w->h - 13 - 10) / FILES_ROW_H;
        for (int i=0;i<files_count && i<maxrows;i++) {
            int ry = cy + i*FILES_ROW_H - 1;
            if (point_in(mx, my, w->x+1, ry, w->w-2, FILES_ROW_H)) {
                files_selected = i;
                int n = fat12_read(files_entries[i].name, files_previewbuf, sizeof(files_previewbuf)-1);
                if (n < 0) n = 0;
                files_previewbuf[n] = 0;
                files_preview = 1;
                return;
            }
        }
    } else {
        if (point_in(mx, my, w->x+w->w-40, w->y+w->h-14, 34, 11))
            files_preview = 0;
    }
}

static void render_sysmon(winrec_t *w) {

    gui_window(w->x, w->y, w->w, w->h, "System Monitor");
    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-13, C_BLACK);

    int cx = w->x+3, cy = w->y+14;

    uint32_t eax,ebx,ecx,edx; char vendor[13];
    __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    kmemcpy(vendor+0,&ebx,4); kmemcpy(vendor+4,&edx,4); kmemcpy(vendor+8,&ecx,4);
    vendor[12]=0;

    gui_printf(cx,cy, C_YELLOW,C_BLACK, "CPU: %s", vendor);  cy += 9;
    gui_printf(cx,cy, C_WHITE, C_BLACK, "Uptime: %ds", (int)timer_seconds()); cy += 9;
    gui_printf(cx,cy, C_WHITE, C_BLACK, "Heap: %uK/%uK", kmalloc_used()/1024, (kmalloc_used()+kmalloc_free())/1024); cy += 9;
    gui_printf(cx,cy, C_WHITE, C_BLACK, "Frames: %u/%u", pmm_used(), pmm_total()); cy += 9;
    gui_printf(cx,cy, C_LIGHT_CYAN,C_BLACK, "Tasks: %d", sched_task_count()); cy += 9;

    int maxrows = (w->y+w->h-9-cy) / 9;
    int shown = 0;
    int cur = sched_current_index();
    for (int i=0;i<sched_task_count() && shown<maxrows;i++) {
        task_t *t = sched_task_at(i);
        if (!t) continue;
        uint8_t fg = (i==cur) ? C_LIGHT_GREEN : C_LIGHT_GREY;
        gui_printf(cx,cy,fg,C_BLACK,"%d %s", t->pid, t->name);
        cy += 9; shown++;
    }
}

void gui_run(void) {
    gui_init();
    term_init();

    for (int i=0;i<WIN_COUNT;i++) wins[i].active = 0;
    zcount = 0;
    wm_open(WIN_TERMINAL, 40, 14, TERM_W, TERM_H);

    int prev_left = 0;
    int dragging = -1, drag_ox = 0, drag_oy = 0;
    int running = 1;

    while (running) {

        gui_draw_desktop();

        for (int i=0;i<zcount;i++) {
            int t = zorder[i];
            winrec_t *w = &wins[t];
            if (!w->active) continue;
            if (t==WIN_TERMINAL) render_terminal(w);
            else if (t==WIN_FILES) render_files(w);
            else if (t==WIN_SYSMON) render_sysmon(w);
        }

        gui_draw_taskbar();

        mouse_state_t *m = mouse_get();
        int mx = m->x, my = m->y;
        if (mx < 0) mx = 0; if (mx >= GUI_WIDTH)  mx = GUI_WIDTH-1;
        if (my < 0) my = 0; if (my >= GUI_HEIGHT) my = GUI_HEIGHT-1;

        gui_draw_cursor(mx, my);

        char key = keyboard_getchar();
        if (key) {
            int top = wm_topmost();
            if (key == 27) {
                if (top >= 0) wm_close(top);
                else running = 0;
            } else if (top == WIN_TERMINAL) {
                if (key == '\n') {
                    char echo[TERM_COLS+3];
                    kstrcpy(echo, "> "); kstrcat(echo, term_input);
                    term_puts(echo);
                    term_exec(term_input);
                    term_input[0]=0; term_icur=0;
                } else if (key == '\b') {
                    if (term_icur > 0) { term_icur--; term_input[term_icur]=0; }
                } else if (term_icur < TERM_COLS-1) {
                    term_input[term_icur++] = key;
                    term_input[term_icur]   = 0;
                }
            }
        }

        int left_now = m->left;
        if (left_now && !prev_left) {

            int hit = -1;
            for (int i=zcount-1;i>=0 && hit<0;i--) {
                int t = zorder[i];
                winrec_t *w = &wins[t];
                if (w->active && point_in(mx,my,w->x,w->y,w->w,w->h)) hit = t;
            }

            if (hit >= 0) {
                winrec_t *w = &wins[hit];
                wm_push_front(hit);
                if (my < w->y+11) {
                    if (mx>=w->x+w->w-11 && mx<w->x+w->w-2) {
                        wm_close(hit);
                    } else {
                        dragging = hit; drag_ox = mx-w->x; drag_oy = my-w->y;
                    }
                } else if (hit == WIN_FILES) {
                    files_handle_click(w, mx, my);
                }
            } else if (my < 10) {
                int th = taskbar_hit(mx, my);
                if (th >= 0) wm_push_front(th);
            } else if (mx < 45 && my >= 18) {
                int iconidx = (my-18)/50;
                if (iconidx==0)      wm_open(WIN_TERMINAL, 40, 14, TERM_W, TERM_H);
                else if (iconidx==1) wm_open(WIN_FILES,    15, 30, 150, 130);
                else if (iconidx==2) wm_open(WIN_SYSMON,  130, 20, 175, 150);
                else if (iconidx==3) running = 0;
            }
        }

        if (left_now && dragging >= 0) {
            winrec_t *w = &wins[dragging];
            int nx = mx-drag_ox, ny = my-drag_oy;
            if (nx < 0) nx = 0;
            if (ny < 10) ny = 10;
            if (nx+w->w > GUI_WIDTH)  nx = GUI_WIDTH-w->w;
            if (ny+w->h > GUI_HEIGHT) ny = GUI_HEIGHT-w->h;
            w->x = nx; w->y = ny;
        }
        if (!left_now) dragging = -1;

        prev_left = left_now;

        __asm__ volatile("hlt");
    }

    gui_exit();
}
