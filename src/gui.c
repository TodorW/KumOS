
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
#include "users.h"
#include "signal.h"
#include "pkg.h"
#include "pkgnet.h"
#include "serial.h"
#include "elf.h"
#include "vfs.h"
#include "net.h"
#include "speaker.h"
#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outw_(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline uint16_t inw_(uint16_t p) { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outl_(uint16_t p, uint32_t v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint32_t inl_(uint16_t p) { uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }

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

/* Bochs/QEMU "dispi" VBE interface: a handful of I/O-port registers that
   let you pick a linear-framebuffer resolution/depth directly, no real-mode
   VBE BIOS calls needed. QEMU's -vga std (and qxl/virtio compat mode)
   emulate this. The framebuffer's physical base comes off the PCI device's
   BAR0 - same PCI-config-space dance net.c already does for the rtl8139. */
#define VBE_IOPORT_INDEX 0x01CE
#define VBE_IOPORT_DATA   0x01CF
#define VBE_IDX_ID        0
#define VBE_IDX_XRES      1
#define VBE_IDX_YRES      2
#define VBE_IDX_BPP       3
#define VBE_IDX_ENABLE    4
#define VBE_DISABLED      0x00
#define VBE_ENABLED       0x01
#define VBE_LFB_ENABLED   0x40

static void vbe_write(uint16_t idx, uint16_t val) {
    outw_(VBE_IOPORT_INDEX, idx);
    outw_(VBE_IOPORT_DATA, val);
}
static uint16_t vbe_read(uint16_t idx) {
    outw_(VBE_IOPORT_INDEX, idx);
    return inw_(VBE_IOPORT_DATA);
}

static uint32_t pci_cfg_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg) {
    outl_(0xCF8, 0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)dev<<11) | ((uint32_t)fn<<8) | (reg & 0xFC));
    return inl_(0xCFC);
}

static uint32_t find_vbe_fb_phys(void) {
    for (uint16_t bd = 0; bd < 256*32; bd++) {
        uint8_t bus = (uint8_t)(bd/32), dev = (uint8_t)(bd%32);
        uint32_t id = pci_cfg_read(bus, dev, 0, 0x00);
        if ((id & 0xFFFF) == 0x1234 && ((id>>16) & 0xFFFF) == 0x1111) {
            uint32_t bar0 = pci_cfg_read(bus, dev, 0, 0x10);
            return bar0 & ~0xFu;
        }
    }
    return 0;
}

static uint32_t fb_phys_addr = 0;
static uint32_t fb_pitch     = 0;
static uint8_t *fb_ptr       = 0;

static int set_vbe_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    if (!fb_phys_addr) {
        fb_phys_addr = find_vbe_fb_phys();
        if (!fb_phys_addr) return -1;
    }
    uint16_t id = vbe_read(VBE_IDX_ID);
    if (id < 0xB0C0 || id > 0xB0C6) return -1;

    vbe_write(VBE_IDX_ENABLE, VBE_DISABLED);
    vbe_write(VBE_IDX_XRES, width);
    vbe_write(VBE_IDX_YRES, height);
    vbe_write(VBE_IDX_BPP,  bpp);
    vbe_write(VBE_IDX_ENABLE, VBE_ENABLED | VBE_LFB_ENABLED);

    fb_pitch = (uint32_t)width * (bpp/8);
    fb_ptr   = (uint8_t*)fb_phys_addr;

    uint32_t size  = fb_pitch * height;
    uint32_t start = fb_phys_addr & ~0xFFFu;
    uint32_t end   = (fb_phys_addr + size + 0xFFF) & ~0xFFFu;
    for (uint32_t p = start; p < end; p += PAGE_SIZE) paging_map(p, p, PAGE_WRITE);

    return 0;
}

static uint32_t gui_pal_rgb[256];

static void build_palette(void) {
    struct { uint8_t r,g,b; } pal[64] = {
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

        {18,28,58}, {16,25,54}, {14,22,50}, {12,20,46},
        {10,18,43}, {9,17,41},  {8,16,40},  {6,13,35},

        {14,18,26}, {13,16,24}, {12,15,22}, {11,14,20},
        {10,13,19}, {9,11,17},  {8,10,15},  {6,9,14},

        {22,34,58}, {19,30,54}, {16,26,50}, {13,22,45},
        {10,18,40}, {7,14,34},  {5,10,28},  {2,6,20},

        {20,55,63}, {17,50,60}, {14,45,57}, {11,40,54},
        {8,35,50},  {6,30,46},  {4,25,42},  {2,20,38},
    };
    for (int i = 0; i < 64; i++)
        gui_pal_rgb[i] = ((uint32_t)(pal[i].r*255/63) << 16) |
                         ((uint32_t)(pal[i].g*255/63) << 8)  |
                          (uint32_t)(pal[i].b*255/63);

    for (int i = 64; i < 256; i++) {
        uint32_t v = (uint32_t)(i/4) * 255/63;
        gui_pal_rgb[i] = (v<<16)|(v<<8)|v;
    }
}

static uint8_t *backbuf = 0;

void gui_init(void) {
    if (set_vbe_mode(GUI_WIDTH, GUI_HEIGHT, 32) != 0)
        serial_printf("[gui] no VBE framebuffer found (need qemu -vga std)\r\n");
    build_palette();
    if (!backbuf) backbuf = (uint8_t*)vmalloc(GUI_WIDTH * GUI_HEIGHT);
    mouse_set_bounds(GUI_WIDTH - 1, GUI_HEIGHT - 1);
}

void gui_exit(void) {
    vbe_write(VBE_IDX_ENABLE, VBE_DISABLED);
    set_mode3h();
    vga_init();
    mouse_set_bounds(79, 24);
}

void gui_pixel(int x, int y, uint8_t c) {
    if (backbuf && (unsigned)x < GUI_WIDTH && (unsigned)y < GUI_HEIGHT)
        backbuf[y * GUI_WIDTH + x] = c;
}

static void gui_flip(void) {
    if (!backbuf || !fb_ptr) return;
    for (uint32_t y = 0; y < GUI_HEIGHT; y++) {
        uint32_t *row = (uint32_t*)(fb_ptr + y*fb_pitch);
        uint8_t  *src = backbuf + y*GUI_WIDTH;
        for (uint32_t x = 0; x < GUI_WIDTH; x++) row[x] = gui_pal_rgb[src[x]];
    }
}

void gui_clear(uint8_t c) {
    if (!backbuf) return;
    for (int i = 0; i < GUI_WIDTH * GUI_HEIGHT; i++) backbuf[i] = c;
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

static void fill_rounded(int x, int y, int w, int h, uint8_t c) {
    for (int row = 0; row < h; row++) {
        int inset = (row==0||row==h-1) ? 2 : (row==1||row==h-2) ? 1 : 0;
        int rw = w - 2*inset;
        if (rw > 0) gui_hline(x+inset, y+row, rw, c);
    }
}

static void fill_rounded_grad(int x, int y, int w, int h, uint8_t grad_base, int grad_n) {
    for (int row = 0; row < h; row++) {
        int inset = (row==0||row==h-1) ? 2 : (row==1||row==h-2) ? 1 : 0;
        int rw = w - 2*inset;
        int band = (row * grad_n) / (h > 1 ? h : 1);
        if (band >= grad_n) band = grad_n-1;
        if (rw > 0) gui_hline(x+inset, y+row, rw, (uint8_t)(grad_base+band));
    }
}

static void outline_rounded(int x, int y, int w, int h, uint8_t c) {
    gui_hline(x+2, y,     w-4, c);
    gui_hline(x+2, y+h-1, w-4, c);
    gui_vline(x,   y+2,   h-4, c);
    gui_vline(x+w-1, y+2, h-4, c);
    gui_pixel(x+1,   y+1,   c); gui_pixel(x+w-2, y+1,   c);
    gui_pixel(x+1,   y+h-2, c); gui_pixel(x+w-2, y+h-2, c);
}

void gui_window(int x, int y, int w, int h, const char *title) {

    fill_rounded(x+3, y+3, w, h, C_SHADOW);

    fill_rounded(x, y, w, h, C_WIN_BG);

    fill_rounded_grad(x, y, w, 11, C_GRAD_TITLE, 8);

    outline_rounded(x, y, w, h, C_WIN_BORDER);

    gui_puts(x+4, y+2, title, C_LIGHT_CYAN, C_GRAD_TITLE+3);

    gui_rect_fill(x+w-11, y+1, 9, 9, C_RED);
    gui_puts(x+w-9, y+2, "X", C_WHITE, C_RED);

    gui_rect_fill(x+w-22, y+1, 9, 9, C_GRAD_ACCENT+2);
    gui_hline(x+w-20, y+6, 5, C_WHITE);

    gui_line(x+w-6, y+h-2, x+w-2, y+h-6, C_WIN_BORDER);
    gui_line(x+w-5, y+h-2, x+w-2, y+h-5, C_WIN_BORDER);
    gui_line(x+w-4, y+h-2, x+w-2, y+h-4, C_WIN_BORDER);
}

void gui_button(int x, int y, int w, int h, const char *label, int pressed) {
    uint8_t bg = pressed ? C_BUTTON : C_BUTTON_HI;
    gui_rect_fill(x, y, w, h, bg);
    gui_rect(x, y, w, h, C_WIN_BORDER);
    int tx = x + (w - (int)kstrlen(label)*8) / 2;
    int ty = y + (h - 8) / 2;
    gui_puts(tx, ty, label, C_WHITE, bg);
}

static void icon_glyph(int x, int y, icon_kind_t kind) {
    /* x,y = top-left of a roughly 20x14 drawing area inside the tile */
    switch (kind) {
    case ICON_TERM:
        gui_rect_fill(x, y, 20, 14, C_BLACK);
        gui_line(x+3, y+4, x+7, y+7, C_LIGHT_GREEN);
        gui_line(x+3, y+10, x+7, y+7, C_LIGHT_GREEN);
        gui_hline(x+9, y+10, 7, C_LIGHT_GREEN);
        break;
    case ICON_FOLDER:
        gui_rect_fill(x, y+3, 8, 3, C_YELLOW);
        gui_rect_fill(x, y+5, 20, 9, C_YELLOW);
        gui_rect(x, y+5, 20, 9, C_BROWN);
        break;
    case ICON_MONITOR:
        gui_rect_fill(x, y, 20, 10, C_BLACK);
        gui_rect(x, y, 20, 10, C_LIGHT_GREY);
        gui_hline(x+2, y+2, 6, C_LIGHT_GREEN);
        gui_hline(x+2, y+5, 10, C_LIGHT_GREEN);
        gui_rect_fill(x+8, y+10, 4, 2, C_LIGHT_GREY);
        gui_hline(x+5, y+13, 10, C_LIGHT_GREY);
        break;
    case ICON_CALC:
        gui_rect_fill(x, y, 20, 14, C_WHITE);
        gui_rect_fill(x+2, y+2, 16, 4, C_LIGHT_GREEN);
        for (int r=0;r<2;r++)
            for (int c=0;c<4;c++)
                gui_rect_fill(x+2+c*4, y+8+r*4, 3, 3, C_DARK_GREY);
        break;
    case ICON_NOTE:
        gui_rect_fill(x, y, 20, 14, C_WHITE);
        gui_hline(x+3, y+3, 14, C_DARK_GREY);
        gui_hline(x+3, y+7, 14, C_DARK_GREY);
        gui_hline(x+3, y+11, 9, C_DARK_GREY);
        break;
    case ICON_EXIT:
        gui_rect(x+2, y, 12, 14, C_LIGHT_GREY);
        gui_vline(x+6, y+3, 8, C_LIGHT_GREY);
        gui_hline(x+13, y+7, 7, C_LIGHT_RED);
        gui_line(x+17, y+4, x+20, y+7, C_LIGHT_RED);
        gui_line(x+17, y+10, x+20, y+7, C_LIGHT_RED);
        break;
    case ICON_PKG:
        gui_rect_fill(x+1, y+3, 18, 11, C_BROWN);
        gui_line(x+1, y+3, x+10, y, C_LIGHT_GREY);
        gui_line(x+19, y+3, x+10, y, C_LIGHT_GREY);
        gui_hline(x+8, y+7, 4, C_YELLOW);
        gui_vline(x+9, y+5, 4, C_YELLOW);
        break;
    }
}

void gui_icon(int x, int y, const char *label, uint8_t icon_color, icon_kind_t kind) {

    fill_rounded(x+1, y+1, 30, 22, C_SHADOW);
    fill_rounded(x, y, 30, 22, icon_color);
    outline_rounded(x, y, 30, 22, C_WIN_BORDER);

    icon_glyph(x+5, y+4, kind);

    int lx = x + (30 - (int)kstrlen(label)*8)/2;
    if (lx < x) lx = x;
    gui_puts(lx, y+23, label, C_WHITE, C_DESKTOP);
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
    for (int row=0;row<CUR_H;row++)
        for (int col=0;col<CUR_W;col++) {
            uint8_t s=cursor_shape[row][col];
            if (s==1) gui_pixel(x+col, y+row, C_WHITE);
            else if(s==2) gui_pixel(x+col, y+row, C_BLACK);
        }
}

static int point_in(int px, int py, int x, int y, int w, int h) {
    return px>=x && px<x+w && py>=y && py<y+h;
}

#define ICON_START_Y 16
#define ICON_SLOT    38

typedef enum {
    WIN_TERMINAL=0, WIN_FILES=1, WIN_SYSMON=2,
    WIN_CALC=3, WIN_EDITOR=4, WIN_PKG=5, WIN_COUNT=6
} wintype_t;

typedef struct { int active, x, y, w, h, minimized, min_w, min_h; } winrec_t;

static winrec_t wins[WIN_COUNT];
static int      zorder[WIN_COUNT];
static int      zcount = 0;

static const char *win_title(int t) {
    switch (t) {
        case WIN_TERMINAL: return "Terminal";
        case WIN_FILES:    return "Files";
        case WIN_SYSMON:   return "System Monitor";
        case WIN_CALC:     return "Calculator";
        case WIN_EDITOR:   return "Notepad";
        case WIN_PKG:      return "Packages";
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

static int wm_topmost(void) {
    for (int i=zcount-1;i>=0;i--)
        if (!wins[zorder[i]].minimized) return zorder[i];
    return -1;
}

static void files_refresh(void);
static void pkg_refresh(void);

static void wm_open(int t, int defx, int defy, int defw, int defh) {
    if (!wins[t].active) {
        wins[t].active = 1;
        wins[t].x = defx; wins[t].y = defy; wins[t].w = defw; wins[t].h = defh;
        wins[t].min_w = defw; wins[t].min_h = defh;
        wins[t].minimized = 0;
        if (t == WIN_FILES) files_refresh();
        if (t == WIN_PKG) pkg_refresh();
    }
    wins[t].minimized = 0;
    wm_push_front(t);
}

static void wm_close(int t) {
    wins[t].active = 0;
    wm_remove_z(t);
}

static void wm_cascade(void) {
    int i = 0;
    for (int z=0; z<zcount; z++) {
        winrec_t *w = &wins[zorder[z]];
        if (!w->active || w->minimized) continue;
        int nx = 44 + i*28, ny = 14 + i*22;
        if (nx + w->w > GUI_WIDTH)  nx = 44;
        if (ny + w->h > GUI_HEIGHT) ny = 14;
        w->x = nx; w->y = ny;
        i++;
    }
}

static void wm_tile(void) {
    int list[WIN_COUNT], n = 0;
    for (int z=0; z<zcount; z++) {
        int t = zorder[z];
        if (wins[t].active && !wins[t].minimized) list[n++] = t;
    }
    if (n == 0) return;
    int cols = 1; while (cols*cols < n) cols++;
    int rows = (n + cols - 1) / cols;
    int areaY = 10, areaH = GUI_HEIGHT - areaY;
    int cellW = GUI_WIDTH / cols, cellH = areaH / rows;
    for (int i=0;i<n;i++) {
        int r = i / cols, c = i % cols;
        winrec_t *w = &wins[list[i]];
        w->x = c*cellW + 2;
        w->y = areaY + r*cellH + 2;
        w->w = cellW - 4; if (w->w < w->min_w) w->w = w->min_w;
        w->h = cellH - 4; if (w->h < w->min_h) w->h = w->min_h;
    }
}

#define TB_TAB_MAX WIN_COUNT
static int tb_tab_type[TB_TAB_MAX];
static int tb_tab_x0[TB_TAB_MAX];
static int tb_tab_x1[TB_TAB_MAX];
static int tb_tab_count = 0;
static int tb_cascade_x0, tb_cascade_x1;
static int tb_tile_x0, tb_tile_x1;

void gui_draw_taskbar(void) {
    for (int y=0;y<9;y++) {
        int band = (y*8)/9; if (band>7) band=7;
        gui_hline(0, y, GUI_WIDTH, (uint8_t)(C_GRAD_TASKBAR+band));
    }
    gui_hline(0, 9, GUI_WIDTH, (uint8_t)(C_GRAD_ACCENT+5));

    fill_rounded(1, 1, 32, 8, (uint8_t)(C_GRAD_ACCENT+2));
    gui_puts(4, 1, "KumOS", C_WHITE, (uint8_t)(C_GRAD_ACCENT+2));

    tb_tab_count = 0;
    int tx = 37;
    int top = wm_topmost();
    for (int i=0;i<zcount;i++) {
        int t = zorder[i];
        if (!wins[t].active) continue;
        int tw = (int)kstrlen(win_title(t))*8 + 6;
        int active = (t==top && !wins[t].minimized);
        uint8_t bg = active ? (uint8_t)(C_GRAD_ACCENT+3) : (uint8_t)(C_GRAD_TASKBAR+1);
        uint8_t fg = wins[t].minimized ? C_LIGHT_GREY : C_WHITE;
        fill_rounded(tx, 0, tw, 9, bg);
        gui_puts(tx+3, 1, win_title(t), fg, bg);
        if (active) gui_hline(tx+2, 8, tw-4, C_WHITE);
        tb_tab_type[tb_tab_count]=t; tb_tab_x0[tb_tab_count]=tx; tb_tab_x1[tb_tab_count]=tx+tw;
        tb_tab_count++;
        tx += tw + 3;
    }

    tb_cascade_x0 = GUI_WIDTH - 178; tb_cascade_x1 = tb_cascade_x0 + 11;
    fill_rounded(tb_cascade_x0, 0, 11, 9, (uint8_t)(C_GRAD_TASKBAR+2));
    gui_putchar(tb_cascade_x0+2, 1, 'C', C_WHITE, (uint8_t)(C_GRAD_TASKBAR+2));

    tb_tile_x0 = tb_cascade_x1 + 2; tb_tile_x1 = tb_tile_x0 + 11;
    fill_rounded(tb_tile_x0, 0, 11, 9, (uint8_t)(C_GRAD_TASKBAR+2));
    gui_putchar(tb_tile_x0+2, 1, 'T', C_WHITE, (uint8_t)(C_GRAD_TASKBAR+2));

    rtc_time_t t = rtc_read();
    char timebuf[10];

    timebuf[0]='0'+t.hour/10;   timebuf[1]='0'+t.hour%10;
    timebuf[2]=':';
    timebuf[3]='0'+t.minute/10; timebuf[4]='0'+t.minute%10;
    timebuf[5]=':';
    timebuf[6]='0'+t.second/10; timebuf[7]='0'+t.second%10;
    timebuf[8]=0;
    gui_puts(GUI_WIDTH-67, 1, timebuf, C_YELLOW, C_GRAD_TASKBAR);

    char ubuf[20];
    uint32_t up=timer_seconds();
    int ui=17; ubuf[18]='s'; ubuf[19]=0;
    if(!up){ubuf[ui--]='0';}else{uint32_t u=up;while(u){ubuf[ui--]='0'+u%10;u/=10;}}
    ubuf[ui--]='p'; ubuf[ui--]='u';
    gui_puts(GUI_WIDTH-130, 1, ubuf+ui+1, C_LIGHT_GREY, C_GRAD_TASKBAR);
}

static int taskbar_hit(int mx, int my) {
    if (my >= 10) return -1;
    for (int i=0;i<tb_tab_count;i++)
        if (mx>=tb_tab_x0[i] && mx<tb_tab_x1[i]) return tb_tab_type[i];
    return -2;
}

void gui_draw_desktop(void) {

    int dh = GUI_HEIGHT - 10;
    for (int y=10;y<GUI_HEIGHT;y++) {
        int band = ((y-10) * 8) / dh;
        if (band > 7) band = 7;
        gui_hline(0, y, GUI_WIDTH, (uint8_t)(C_GRAD_SKY+band));
    }

    gui_icon(8, ICON_START_Y + 0*ICON_SLOT, "Term",  C_TEAL,       ICON_TERM);
    gui_icon(8, ICON_START_Y + 1*ICON_SLOT, "Files", C_NAVY,       ICON_FOLDER);
    gui_icon(8, ICON_START_Y + 2*ICON_SLOT, "Sys",   C_PURPLE,     ICON_MONITOR);
    gui_icon(8, ICON_START_Y + 3*ICON_SLOT, "Calc",  C_BROWN,      ICON_CALC);
    gui_icon(8, ICON_START_Y + 4*ICON_SLOT, "Note",  C_LIGHT_BLUE, ICON_NOTE);
    gui_icon(8, ICON_START_Y + 5*ICON_SLOT, "Pkgs",  C_TEAL,       ICON_PKG);
    gui_icon(8, ICON_START_Y + 6*ICON_SLOT, "Exit",  C_DARK_GREY,  ICON_EXIT);

    gui_printf(52, 18, C_LIGHT_GREY, C_DESKTOP,
               "Click icons to open apps");
    gui_printf(52, 28, C_DARK_GREY,  C_DESKTOP,
               "Drag bar to move, Esc to close");
}

#define TERM_W    680
#define TERM_H    460
#define TERM_ROWS  48
#define TERM_COLS  84

#define TERM_BG       C_DARK_BLUE
#define TERM_FG       C_WHITE
#define TERM_FG_ECHO  C_LIGHT_CYAN
#define TERM_FG_ERR   C_LIGHT_RED
#define TERM_FG_OK    C_LIGHT_GREEN
#define TERM_FG_INFO  C_YELLOW

static char    term_lines[TERM_ROWS][TERM_COLS+1];
static uint8_t term_colors[TERM_ROWS];
static int     term_row = 0;
static char    term_input[TERM_COLS+1];
static int     term_icur = 0;

#define TERM_HIST 8
static char term_hist[TERM_HIST][TERM_COLS+1];
static int  term_hist_count = 0;

static void term_init(void) {
    for (int i=0;i<TERM_ROWS;i++) { term_lines[i][0]=0; term_colors[i]=TERM_FG; }
    kstrcpy(term_lines[0], "KumOS GUI terminal"); term_colors[0]=TERM_FG_INFO;
    kstrcpy(term_lines[1], "help for commands, exit to close"); term_colors[1]=TERM_FG_INFO;
    term_row = 2;
    term_input[0]=0; term_icur=0;
    term_hist_count = 0;
}

static void term_scroll(void) {
    for (int i=0;i<TERM_ROWS-1;i++) {
        kstrcpy(term_lines[i], term_lines[i+1]);
        term_colors[i] = term_colors[i+1];
    }
    term_lines[TERM_ROWS-1][0]=0;
    if (term_row > 0) term_row--;
}

static void term_puts_c(const char *s, uint8_t color) {
    if (term_row >= TERM_ROWS) term_scroll();
    kstrcpy(term_lines[term_row], s);
    term_colors[term_row] = color;
    term_row++;
}

static void term_puts(const char *s) { term_puts_c(s, TERM_FG); }

/* captures a spawned ring-3 process's stdout/stderr into the terminal -
   fd 1/2 normally go to vga_putchar (VGA text mode), which isn't the
   visible surface while we're in graphics mode, so nothing would show up
   otherwise. registered as the stdout sink only for the duration of an
   exec/run command (see below), one line at a time. */
static char    term_exec_linebuf[TERM_COLS+1];
static int     term_exec_linelen = 0;

static void term_exec_output(char c) {
    if (c == '\n') {
        term_exec_linebuf[term_exec_linelen] = 0;
        term_puts(term_exec_linebuf);
        term_exec_linelen = 0;
    } else if (c != '\r' && term_exec_linelen < TERM_COLS) {
        term_exec_linebuf[term_exec_linelen++] = c;
    }
}

static void term_run_elf(elf_load_result_t *r, const char *name) {
    if (r->error != 0) {
        char line[TERM_COLS+1];
        kstrcpy(line, "bad ELF, error "); kitoa((uint32_t)(-r->error), line+kstrlen(line), 10);
        term_puts_c(line, TERM_FG_ERR);
        return;
    }
    int pid = elf_spawn(name, r);
    if (pid < 0) { term_puts_c("spawn failed", TERM_FG_ERR); return; }

    term_exec_linelen = 0;
    vfs_set_stdout_sink(term_exec_output);
    int code = sched_waitpid(pid);
    vfs_set_stdout_sink(0);
    if (term_exec_linelen > 0) {
        term_exec_linebuf[term_exec_linelen] = 0;
        term_puts(term_exec_linebuf);
        term_exec_linelen = 0;
    }

    char line[TERM_COLS+1];
    kstrcpy(line, "exited, code "); kitoa((uint32_t)code, line+kstrlen(line), 10);
    term_puts_c(line, TERM_FG_INFO);
}

static void term_split(const char *line, char *first, char *rest, int firstlen) {
    int i=0;
    while (*line && *line!=' ' && i<firstlen-1) first[i++]=*line++;
    first[i]=0;
    while (*line==' ') line++;
    kstrcpy(rest, line);
}

static void term_itoa_signed(int v, char *out) {
    int pos=0;
    if (v<0) out[pos++]='-';
    char tmp[12];
    kitoa((uint32_t)(v<0?-v:v), tmp, 10);
    kstrcpy(out+pos, tmp);
}

static int term_calc(const char *e) {
    int a=0,b=0,neg=0; const char *p=e;
    while(*p==' ')p++; if(*p=='-'){neg=1;p++;}
    while(*p>='0'&&*p<='9'){a=a*10+(*p-'0');p++;} if(neg)a=-a;
    while(*p==' ')p++; if(!*p)return a;
    char op=*p++; while(*p==' ')p++;
    neg=0; if(*p=='-'){neg=1;p++;}
    while(*p>='0'&&*p<='9'){b=b*10+(*p-'0');p++;} if(neg)b=-b;
    switch(op){case '+':return a+b;case '-':return a-b;
               case '*':return a*b;case '/':return b?a/b:0;
               case '%':return b?a%b:0;} return 0;
}

static void render_terminal(winrec_t *w) {

    gui_window(w->x, w->y, w->w, w->h, "Terminal");

    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-22, TERM_BG);

    int tx = w->x+4, ty = w->y+13;
    for (int i=0;i<TERM_ROWS-1;i++)
        if (term_lines[i][0])
            gui_puts(tx, ty + i*9, term_lines[i], term_colors[i], TERM_BG);

    int iy = ty + (TERM_ROWS-1)*9;
    gui_rect_fill(w->x+1, iy-1, w->w-2, 10, TERM_BG);
    gui_puts(tx, iy, "> ", TERM_FG_INFO, TERM_BG);
    gui_puts(tx+16, iy, term_input, TERM_FG, TERM_BG);

    if ((timer_ticks() / 50) & 1)
        gui_rect_fill(tx+16+term_icur*8, iy, 7, 8, C_LIGHT_GREY);
}

static void term_exec(const char *cmd) {
    char line[TERM_COLS+1];
    char num[12];

    if (*cmd) {
        if (term_hist_count < TERM_HIST) kstrcpy(term_hist[term_hist_count++], cmd);
        else {
            for (int i=0;i<TERM_HIST-1;i++) kstrcpy(term_hist[i], term_hist[i+1]);
            kstrcpy(term_hist[TERM_HIST-1], cmd);
        }
    }

    if (kstrcmp(cmd,"help")==0) {
        term_puts_c("help date uptime clear history", TERM_FG_INFO);
        term_puts_c("ls cat touch write rm <f>", TERM_FG_INFO);
        term_puts_c("meminfo cpuinfo ps kill whoami", TERM_FG_INFO);
        term_puts_c("echo calc pkg exit", TERM_FG_INFO);
        term_puts_c("run <pkg>  exec <file.elf>", TERM_FG_INFO);
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
    } else if (kstrcmp(cmd,"history")==0) {
        for (int i=0;i<term_hist_count;i++) {
            char n2[4]; kitoa((uint32_t)(i+1),n2,10);
            kstrcpy(line,n2); kstrcat(line,". "); kstrcat(line,term_hist[i]);
            term_puts(line);
        }
    } else if (kstrcmp(cmd,"whoami")==0) {
        user_t *u = users_get_current();
        term_puts(u ? u->name : "unknown");
    } else if (kstrcmp(cmd,"cpuinfo")==0) {
        uint32_t eax,ebx,ecx,edx; char vendor[13];
        __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
        kmemcpy(vendor,&ebx,4); kmemcpy(vendor+4,&edx,4); kmemcpy(vendor+8,&ecx,4); vendor[12]=0;
        kstrcpy(line,"CPU: "); kstrcat(line,vendor); term_puts(line);
        __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(1));
        kstrcpy(line,"Features: ");
        if(edx&(1<<0))  kstrcat(line,"FPU ");
        if(edx&(1<<23)) kstrcat(line,"MMX ");
        if(edx&(1<<25)) kstrcat(line,"SSE ");
        if(edx&(1<<26)) kstrcat(line,"SSE2 ");
        term_puts(line);
    } else if (kstartswith(cmd,"calc ")) {
        kstrcpy(line, "= "); term_itoa_signed(term_calc(cmd+5), line+2);
        term_puts_c(line, TERM_FG_OK);
    } else if (kstartswith(cmd,"kill ")) {
        const char *p = cmd+5; int pid=0;
        while(*p>='0'&&*p<='9'){pid=pid*10+(*p-'0');p++;}
        while(*p==' ')p++;
        int sig=15; if(*p){sig=0;while(*p>='0'&&*p<='9'){sig=sig*10+(*p-'0');p++;}}
        if (!pid) term_puts_c("Usage: kill <pid> [sig]", TERM_FG_ERR);
        else {
            signal_send(pid, sig);
            kstrcpy(line,"Signal sent to PID "); kitoa((uint32_t)pid,num,10); kstrcat(line,num);
            term_puts_c(line, TERM_FG_OK);
        }
    } else if (kstrcmp(cmd,"pkg")==0 || kstrcmp(cmd,"pkg list")==0) {
        for (int i=0;i<pkg_count();i++) {
            kstrcpy(line, pkg_is_installed(i) ? "[x] " : "[ ] ");
            kstrcat(line, pkg_name_at(i));
            term_puts(line);
        }
    } else if (kstartswith(cmd,"pkg install ")) {
        int r = pkg_install(cmd+12);
        if (r==0) { kstrcpy(line,"Installed "); kstrcat(line,cmd+12); term_puts_c(line, TERM_FG_OK); }
        else if (r==-1) term_puts_c("Unknown package", TERM_FG_ERR);
        else if (r==-2) term_puts_c("No disk mounted", TERM_FG_ERR);
        else term_puts_c("Install failed", TERM_FG_ERR);
    } else if (kstartswith(cmd,"pkg remove ")) {
        int r = pkg_remove(cmd+11);
        if (r==0) { kstrcpy(line,"Removed "); kstrcat(line,cmd+11); term_puts_c(line, TERM_FG_OK); }
        else if (r==-1) term_puts_c("Unknown package", TERM_FG_ERR);
        else if (r==-2) term_puts_c("No disk mounted", TERM_FG_ERR);
        else term_puts_c("Not installed", TERM_FG_ERR);
    } else if (kstartswith(cmd,"run ")) {
        int i = pkg_find_index(cmd+4);
        if (i < 0) { term_puts_c("Unknown program (try 'pkg install' first)", TERM_FG_ERR); }
        else {
            const uint8_t *start, *end;
            pkg_blob_at(i, &start, &end);
            elf_load_result_t r = elf_load_mem(start, (uint32_t)(end - start));
            term_run_elf(&r, cmd+4);
        }
    } else if (kstartswith(cmd,"exec ")) {
        if (!fat12_mounted()) { term_puts_c("No disk mounted", TERM_FG_ERR); }
        else {
            elf_load_result_t r = elf_load_disk(cmd+5);
            term_run_elf(&r, cmd+5);
        }
    } else if (kstrcmp(cmd,"ls")==0) {
        if (!fat12_mounted()) {
            term_puts_c("No disk mounted", TERM_FG_ERR);
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
    } else if (kstartswith(cmd,"echo ")) {
        term_puts(cmd+5);
    } else if (kstartswith(cmd,"touch ")) {
        if (!fat12_mounted()) term_puts_c("No disk mounted", TERM_FG_ERR);
        else if (fat12_write(cmd+6, "", 0)==0) {
            kstrcpy(line,"Created "); kstrcat(line,cmd+6); term_puts_c(line, TERM_FG_OK);
        } else term_puts_c("touch failed", TERM_FG_ERR);
    } else if (kstartswith(cmd,"write ")) {
        char fn[16], data[TERM_COLS+1];
        term_split(cmd+6, fn, data, 16);
        if (!*fn) term_puts_c("Usage: write <f> <data>", TERM_FG_ERR);
        else if (!fat12_mounted()) term_puts_c("No disk mounted", TERM_FG_ERR);
        else if (fat12_write(fn, data, kstrlen(data))==0) term_puts_c("Written", TERM_FG_OK);
        else term_puts_c("write failed", TERM_FG_ERR);
    } else if (kstartswith(cmd,"rm ")) {
        if (!fat12_mounted()) term_puts_c("No disk mounted", TERM_FG_ERR);
        else if (fat12_delete(cmd+3)==0) {
            kstrcpy(line,"Deleted "); kstrcat(line,cmd+3); term_puts_c(line, TERM_FG_OK);
        } else term_puts_c("rm: not found", TERM_FG_ERR);
    } else if (kstartswith(cmd,"cat ")) {
        if (!fat12_mounted()) {
            term_puts_c("No disk mounted", TERM_FG_ERR);
        } else {
            uint8_t buf[128];
            int n = fat12_read(cmd+4, buf, sizeof(buf)-1);
            if (n < 0) {
                kstrcpy(line,"not found: "); kstrcat(line,cmd+4); term_puts_c(line, TERM_FG_ERR);
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
        term_puts_c(line, TERM_FG_ERR);
        speaker_beep(300, 100);
    }
}

#define FILES_MAX    40
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

    if (net_ready()) {
        uint32_t ip = net_get_ip();
        uint32_t txp, rxp; net_get_stats(&txp, &rxp);
        gui_printf(cx,cy, C_LIGHT_GREEN,C_BLACK, "Net: %d.%d.%d.%d",
                   (int)(ip>>24)&0xFF, (int)(ip>>16)&0xFF, (int)(ip>>8)&0xFF, (int)ip&0xFF); cy += 9;
        gui_printf(cx,cy, C_WHITE,C_BLACK, "TX:%u RX:%u pkts", txp, rxp); cy += 9;
    } else {
        gui_printf(cx,cy, C_LIGHT_RED,C_BLACK, "Net: no NIC"); cy += 9;
    }

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

static int  calc_acc = 0;
static int  calc_entry = 0;
static char calc_op = 0;
static int  calc_entry_active = 0;
static int  calc_error = 0;

static const char *calc_labels[4][4] = {
    {"7","8","9","/"},
    {"4","5","6","*"},
    {"1","2","3","-"},
    {"0","C","=","+"},
};

#define CALC_BW  30
#define CALC_BH  22
#define CALC_GAP  3

static void calc_grid_origin(winrec_t *w, int *gx, int *gy) {
    *gx = w->x+4; *gy = w->y+34;
}

static int calc_apply(int a, char op, int b) {
    switch (op) {
        case '+': return a+b;
        case '-': return a-b;
        case '*': return a*b;
        case '/': if (!b) { calc_error = 1; return 0; } return a/b;
    }
    return b;
}

static void calc_press(const char *label) {
    char c = label[0];
    if (c>='0' && c<='9') {
        if (!calc_entry_active) { calc_entry = 0; calc_entry_active = 1; }
        if (calc_entry < 100000000) calc_entry = calc_entry*10 + (c-'0');
    } else if (c=='C') {
        calc_acc=0; calc_entry=0; calc_op=0; calc_entry_active=0; calc_error=0;
    } else if (c=='=') {
        if (calc_op) { calc_acc = calc_apply(calc_acc, calc_op, calc_entry); calc_op = 0; }
        else calc_acc = calc_entry;
        calc_entry_active = 0;
    } else {
        if (calc_op && calc_entry_active) calc_acc = calc_apply(calc_acc, calc_op, calc_entry);
        else if (!calc_op && calc_entry_active) calc_acc = calc_entry;
        calc_op = c;
        calc_entry_active = 0;
        calc_entry = 0;
    }
}

static void render_calc(winrec_t *w) {
    gui_window(w->x, w->y, w->w, w->h, "Calculator");
    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-13, C_WIN_BG);

    gui_rect_fill(w->x+4, w->y+16, w->w-8, 14, C_BLACK);
    char dispbuf[16];
    if (calc_error) {
        kstrcpy(dispbuf, "Error");
    } else {
        int dv = calc_entry_active ? calc_entry : calc_acc;
        int pos = 0;
        if (dv < 0) dispbuf[pos++] = '-';
        char tmp[12];
        kitoa((uint32_t)(dv<0 ? -dv : dv), tmp, 10);
        kstrcpy(dispbuf+pos, tmp);
    }
    gui_puts(w->x+6, w->y+18, dispbuf, C_LIGHT_GREEN, C_BLACK);

    int gx, gy;
    calc_grid_origin(w, &gx, &gy);
    for (int r=0;r<4;r++)
        for (int c2=0;c2<4;c2++)
            gui_button(gx+c2*(CALC_BW+CALC_GAP), gy+r*(CALC_BH+CALC_GAP),
                       CALC_BW, CALC_BH, calc_labels[r][c2], 0);
}

static void calc_handle_click(winrec_t *w, int mx, int my) {
    int gx, gy;
    calc_grid_origin(w, &gx, &gy);
    for (int r=0;r<4;r++)
        for (int c2=0;c2<4;c2++) {
            int bx = gx+c2*(CALC_BW+CALC_GAP), by = gy+r*(CALC_BH+CALC_GAP);
            if (point_in(mx, my, bx, by, CALC_BW, CALC_BH)) { calc_press(calc_labels[r][c2]); return; }
        }
}

#define EDIT_ROWS 20
#define EDIT_COLS 50

static char editor_lines[EDIT_ROWS][EDIT_COLS+1];
static int  editor_row = 0, editor_col = 0;
static char editor_status[20] = "";

static void editor_init_buf(void) {
    for (int i=0;i<EDIT_ROWS;i++) editor_lines[i][0] = 0;
    editor_row = 0; editor_col = 0;
}

static void render_editor(winrec_t *w) {
    gui_window(w->x, w->y, w->w, w->h, "Notepad");
    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-13, C_WHITE);

    int tx = w->x+3, ty = w->y+14;
    for (int i=0;i<EDIT_ROWS;i++)
        gui_puts(tx, ty+i*9, editor_lines[i], C_BLACK, C_WHITE);

    if ((timer_ticks()/50) & 1)
        gui_rect_fill(tx+editor_col*8, ty+editor_row*9, 7, 8, C_LIGHT_GREY);

    gui_button(w->x+4,  w->y+w->h-16, 40, 12, "Save", 0);
    gui_button(w->x+48, w->y+w->h-16, 40, 12, "Load", 0);
    gui_puts(w->x+94, w->y+w->h-14, editor_status, C_DARK_GREY, C_WIN_BG);
}

static void editor_handle_click(winrec_t *w, int mx, int my) {
    if (point_in(mx, my, w->x+4, w->y+w->h-16, 40, 12)) {
        char buf[EDIT_ROWS*(EDIT_COLS+1)]; int n=0;
        for (int i=0;i<=editor_row;i++) {
            const char *p = editor_lines[i];
            while (*p) buf[n++] = *p++;
            if (i < editor_row) buf[n++] = '\n';
        }
        if (!fat12_mounted()) kstrcpy(editor_status, "No disk");
        else if (fat12_write("NOTES.TXT", buf, (uint32_t)n) == 0) kstrcpy(editor_status, "Saved!");
        else kstrcpy(editor_status, "Save failed");
    } else if (point_in(mx, my, w->x+48, w->y+w->h-16, 40, 12)) {
        if (!fat12_mounted()) { kstrcpy(editor_status, "No disk"); return; }
        uint8_t fbuf[EDIT_ROWS*(EDIT_COLS+1)];
        int n = fat12_read("NOTES.TXT", fbuf, sizeof(fbuf)-1);
        if (n < 0) { kstrcpy(editor_status, "Not found"); return; }
        fbuf[n] = 0;
        editor_init_buf();
        char *p = (char*)fbuf;
        while (*p && editor_row < EDIT_ROWS) {
            if (*p == '\n') { editor_row++; editor_col = 0; p++; continue; }
            if (editor_col < EDIT_COLS-1) {
                editor_lines[editor_row][editor_col++] = *p;
                editor_lines[editor_row][editor_col] = 0;
            }
            p++;
        }
        kstrcpy(editor_status, "Loaded!");
    }
}

static int  pkg_selected = -1;
static char pkg_status[24] = "";

static void pkg_refresh(void) { pkg_selected = -1; pkg_status[0] = 0; }

static int pkgwin_total(void) { return pkg_count() + pkgnet_count(); }

static const char *pkgwin_name(int i) {
    int n = pkg_count();
    return (i < n) ? pkg_name_at(i) : pkgnet_name_at(i-n);
}

static int pkgwin_installed(int i) {
    int n = pkg_count();
    if (i < n) return pkg_is_installed(i);
    if (!fat12_mounted()) return 0;
    fat12_entry_t ents[64];
    int cnt = fat12_list(ents, 64);
    const char *fname = pkgnet_fname_at(i-n);
    for (int j=0;j<cnt;j++) if (kstrcmp(ents[j].name, fname)==0) return 1;
    return 0;
}

static const char *pkgwin_fname(int i) {
    int n = pkg_count();
    return (i < n) ? pkg_fname_at(i) : pkgnet_fname_at(i-n);
}

#define PKGWIN_ROW_H 9

static void render_pkg(winrec_t *w) {
    gui_window(w->x, w->y, w->w, w->h, "Packages");
    gui_rect_fill(w->x+1, w->y+12, w->w-2, w->h-13, C_BLACK);

    int cx = w->x+3, cy = w->y+13;
    int total = pkgwin_total();
    int by = w->y+w->h-20;
    int maxrows = (by - cy) / PKGWIN_ROW_H;

    for (int i=0;i<total && i<maxrows;i++) {
        int ry = cy + i*PKGWIN_ROW_H - 1;
        uint8_t fg = (i==pkg_selected) ? C_BLACK : (pkgwin_installed(i) ? C_LIGHT_GREEN : C_LIGHT_GREY);
        uint8_t bg = (i==pkg_selected) ? (uint8_t)(C_GRAD_ACCENT+3) : C_BLACK;
        if (i==pkg_selected) gui_rect_fill(w->x+1, ry, w->w-2, PKGWIN_ROW_H, bg);
        gui_puts(cx, cy+i*PKGWIN_ROW_H, pkgwin_name(i), fg, bg);
    }

    gui_button(w->x+3,   by, 44, 11, "Update", 0);
    gui_button(w->x+49,  by, 36, 11, "Instl",  0);
    gui_button(w->x+87,  by, 40, 11, "Rmove",  0);
    gui_button(w->x+129, by, 36, 11, "Run",    0);
    gui_puts(w->x+3, by+12, pkg_status, C_YELLOW, C_WIN_BG);
}

static void pkg_handle_click(winrec_t *w, int mx, int my) {
    int cy = w->y+13;
    int total = pkgwin_total();
    int by = w->y+w->h-20;
    int maxrows = (by - cy) / PKGWIN_ROW_H;

    if (point_in(mx, my, w->x+3, by, 44, 11)) {
        int n = pkgnet_update();
        kstrcpy(pkg_status, n < 0 ? "update failed" : "index updated");
        return;
    }
    if (point_in(mx, my, w->x+49, by, 40, 11)) {
        if (pkg_selected >= 0) {
            const char *nm = pkgwin_name(pkg_selected);
            int r = pkg_install(nm);
            if (r == -1) r = pkgnet_install(nm);
            kstrcpy(pkg_status, r == 0 ? "installed" : "install failed");
        }
        return;
    }
    if (point_in(mx, my, w->x+87, by, 40, 11)) {
        if (pkg_selected >= 0) {
            int r = pkg_remove(pkgwin_name(pkg_selected));
            kstrcpy(pkg_status, r == 0 ? "removed" : "remove failed");
        }
        return;
    }
    if (point_in(mx, my, w->x+129, by, 36, 11)) {
        if (pkg_selected < 0) { kstrcpy(pkg_status, "select one first"); return; }
        if (!pkgwin_installed(pkg_selected)) { kstrcpy(pkg_status, "install it first"); return; }

        wm_open(WIN_TERMINAL, 40, 20, TERM_W, TERM_H);

        char cmdbuf[TERM_COLS+1];
        kstrcpy(cmdbuf, "exec "); kstrcat(cmdbuf, pkgwin_fname(pkg_selected));
        char echo[TERM_COLS+3];
        kstrcpy(echo, "> "); kstrcat(echo, cmdbuf);
        term_puts_c(echo, TERM_FG_ECHO);
        term_exec(cmdbuf);

        kstrcpy(pkg_status, "ran in Terminal");
        return;
    }
    for (int i=0;i<total && i<maxrows;i++) {
        int ry = cy + i*PKGWIN_ROW_H - 1;
        if (point_in(mx, my, w->x+1, ry, w->w-2, PKGWIN_ROW_H)) { pkg_selected = i; return; }
    }
}

#define LAUNCH_ROW_H 16
#define LAUNCH_W     168
#define LAUNCH_MAX   16

typedef struct { const char *label; icon_kind_t icon; int wintype; int is_pkg; int pkg_idx; } launch_item_t;

static int launcher_open = 0;

static int launcher_build(launch_item_t *items, int max) {
    int n = 0;
    if (n<max) items[n++] = (launch_item_t){"Terminal",       ICON_TERM,    WIN_TERMINAL, 0, 0};
    if (n<max) items[n++] = (launch_item_t){"Files",          ICON_FOLDER,  WIN_FILES,    0, 0};
    if (n<max) items[n++] = (launch_item_t){"System Monitor", ICON_MONITOR, WIN_SYSMON,   0, 0};
    if (n<max) items[n++] = (launch_item_t){"Calculator",     ICON_CALC,    WIN_CALC,     0, 0};
    if (n<max) items[n++] = (launch_item_t){"Notepad",        ICON_NOTE,    WIN_EDITOR,   0, 0};
    if (n<max) items[n++] = (launch_item_t){"Packages",       ICON_PKG,     WIN_PKG,      0, 0};
    int total = pkgwin_total();
    for (int i=0;i<total && n<max;i++) {
        if (!pkgwin_installed(i)) continue;
        items[n].label = pkgwin_name(i);
        items[n].icon = ICON_PKG;
        items[n].wintype = -1;
        items[n].is_pkg = 1;
        items[n].pkg_idx = i;
        n++;
    }
    return n;
}

static void launcher_open_wintype(int t) {
    switch (t) {
        case WIN_TERMINAL: wm_open(WIN_TERMINAL, 40,  20, TERM_W, TERM_H); break;
        case WIN_FILES:    wm_open(WIN_FILES,   760,  20, 230, 290);       break;
        case WIN_SYSMON:   wm_open(WIN_SYSMON,  760, 330, 230, 290);       break;
        case WIN_CALC:     wm_open(WIN_CALC,    480, 500, 190, 230);       break;
        case WIN_EDITOR:   wm_open(WIN_EDITOR,   40, 500, 420, 230);       break;
        case WIN_PKG:      wm_open(WIN_PKG,     690, 500, 300, 230);       break;
    }
}

static void launcher_run_pkg(int pkg_idx) {
    wm_open(WIN_TERMINAL, 40, 20, TERM_W, TERM_H);
    char cmdbuf[TERM_COLS+1];
    kstrcpy(cmdbuf, "exec "); kstrcat(cmdbuf, pkgwin_fname(pkg_idx));
    char echo[TERM_COLS+3];
    kstrcpy(echo, "> "); kstrcat(echo, cmdbuf);
    term_puts_c(echo, TERM_FG_ECHO);
    term_exec(cmdbuf);
}

static void draw_launcher(void) {
    launch_item_t items[LAUNCH_MAX];
    int n = launcher_build(items, LAUNCH_MAX);
    int h = n*LAUNCH_ROW_H + 6;
    int x = 1, y = 11;

    fill_rounded(x+2, y+2, LAUNCH_W, h, C_SHADOW);
    fill_rounded(x, y, LAUNCH_W, h, C_WIN_BG);
    outline_rounded(x, y, LAUNCH_W, h, C_WIN_BORDER);

    for (int i=0;i<n;i++) {
        int ry = y+3+i*LAUNCH_ROW_H;
        if (i == 6 && n > 6) gui_hline(x+4, ry-2, LAUNCH_W-8, C_WIN_BORDER);
        icon_glyph(x+6, ry+1, items[i].icon);
        gui_puts(x+30, ry+4, items[i].label, C_WHITE, C_WIN_BG);
    }
}

/* -2 = click outside the panel, -1 = inside panel but not on a row, >=0 = row index */
static int launcher_hit(int mx, int my, int n) {
    int h = n*LAUNCH_ROW_H + 6;
    if (!point_in(mx, my, 1, 11, LAUNCH_W, h)) return -2;
    int row = (my - 14) / LAUNCH_ROW_H;
    return (row >= 0 && row < n) ? row : -1;
}

#define CTXMENU_ROW_H 16
#define CTXMENU_W     140
#define CTXMENU_ITEMS 3

static int ctxmenu_open = 0;
static int ctxmenu_x = 0, ctxmenu_y = 0;
static const char *ctxmenu_labels[CTXMENU_ITEMS] = {"New Terminal","Cascade Windows","Tile Windows"};

static void ctxmenu_rect(int *x, int *y, int *h) {
    *h = CTXMENU_ITEMS*CTXMENU_ROW_H + 6;
    *x = ctxmenu_x; *y = ctxmenu_y;
    if (*x + CTXMENU_W > GUI_WIDTH)  *x = GUI_WIDTH  - CTXMENU_W;
    if (*y + *h > GUI_HEIGHT)        *y = GUI_HEIGHT - *h;
}

static void draw_ctxmenu(void) {
    int x, y, h; ctxmenu_rect(&x, &y, &h);
    fill_rounded(x+2, y+2, CTXMENU_W, h, C_SHADOW);
    fill_rounded(x, y, CTXMENU_W, h, C_WIN_BG);
    outline_rounded(x, y, CTXMENU_W, h, C_WIN_BORDER);
    for (int i=0;i<CTXMENU_ITEMS;i++) {
        int ry = y+3+i*CTXMENU_ROW_H;
        gui_puts(x+8, ry+4, ctxmenu_labels[i], C_WHITE, C_WIN_BG);
    }
}

/* -2 = click outside the menu, -1 = inside but not on a row, >=0 = row index */
static int ctxmenu_hit(int mx, int my) {
    int x, y, h; ctxmenu_rect(&x, &y, &h);
    if (!point_in(mx, my, x, y, CTXMENU_W, h)) return -2;
    int row = (my - (y+3)) / CTXMENU_ROW_H;
    return (row >= 0 && row < CTXMENU_ITEMS) ? row : -1;
}

void gui_run(void) {
    gui_init();
    term_init();
    editor_init_buf();

    for (int i=0;i<WIN_COUNT;i++) wins[i].active = 0;
    zcount = 0;
    wm_open(WIN_TERMINAL, 40, 20, TERM_W, TERM_H);

    int prev_left = 0, prev_right = 0;
    int dragging = -1, drag_ox = 0, drag_oy = 0;
    int resizing = -1, resize_ow = 0, resize_oh = 0, resize_mx = 0, resize_my = 0;
    int running = 1;

    while (running) {

        gui_draw_desktop();

        for (int i=0;i<zcount;i++) {
            int t = zorder[i];
            winrec_t *w = &wins[t];
            if (!w->active || w->minimized) continue;
            if (t==WIN_TERMINAL) render_terminal(w);
            else if (t==WIN_FILES) render_files(w);
            else if (t==WIN_SYSMON) render_sysmon(w);
            else if (t==WIN_CALC) render_calc(w);
            else if (t==WIN_EDITOR) render_editor(w);
            else if (t==WIN_PKG) render_pkg(w);
        }

        gui_draw_taskbar();
        if (launcher_open) draw_launcher();
        if (ctxmenu_open) draw_ctxmenu();

        mouse_state_t *m = mouse_get();
        int mx = m->x, my = m->y;
        if (mx < 0) mx = 0; if (mx >= GUI_WIDTH)  mx = GUI_WIDTH-1;
        if (my < 0) my = 0; if (my >= GUI_HEIGHT) my = GUI_HEIGHT-1;

        gui_draw_cursor(mx, my);
        gui_flip();

        char key = keyboard_getchar();
        if (key) {
            int top = wm_topmost();
            if (key == 27) {
                if (ctxmenu_open) ctxmenu_open = 0;
                else if (launcher_open) launcher_open = 0;
                else if (top >= 0) wm_close(top);
                else running = 0;
            } else if (top == WIN_TERMINAL) {
                if (key == '\n') {
                    char echo[TERM_COLS+3];
                    kstrcpy(echo, "> "); kstrcat(echo, term_input);
                    term_puts_c(echo, TERM_FG_ECHO);
                    term_exec(term_input);
                    term_input[0]=0; term_icur=0;
                } else if (key == '\b') {
                    if (term_icur > 0) { term_icur--; term_input[term_icur]=0; }
                } else if (term_icur < TERM_COLS-1) {
                    term_input[term_icur++] = key;
                    term_input[term_icur]   = 0;
                }
            } else if (top == WIN_EDITOR) {
                if (key == '\n') {
                    if (editor_row < EDIT_ROWS-1) {
                        editor_row++; editor_col = 0; editor_lines[editor_row][0] = 0;
                    }
                } else if (key == '\b') {
                    if (editor_col > 0) {
                        editor_col--; editor_lines[editor_row][editor_col] = 0;
                    } else if (editor_row > 0) {
                        editor_row--; editor_col = (int)kstrlen(editor_lines[editor_row]);
                    }
                } else if (editor_col < EDIT_COLS-1) {
                    editor_lines[editor_row][editor_col++] = key;
                    editor_lines[editor_row][editor_col]   = 0;
                }
            }
        }

        int right_now = m->right;
        if (right_now && !prev_right && !ctxmenu_open && !launcher_open && my >= 10) {
            int hitw = -1;
            for (int i=zcount-1;i>=0 && hitw<0;i--) {
                int t = zorder[i]; winrec_t *w = &wins[t];
                if (w->active && !w->minimized && point_in(mx,my,w->x,w->y,w->w,w->h)) hitw = t;
            }
            if (hitw < 0) { ctxmenu_open = 1; ctxmenu_x = mx; ctxmenu_y = my; }
        }
        prev_right = right_now;

        int left_now = m->left;
        if (left_now && !prev_left && ctxmenu_open) {
            int row = ctxmenu_hit(mx, my);
            ctxmenu_open = 0;
            if (row == 0) wm_open(WIN_TERMINAL, 40, 20, TERM_W, TERM_H);
            else if (row == 1) wm_cascade();
            else if (row == 2) wm_tile();
        } else if (left_now && !prev_left && launcher_open) {
            launch_item_t items[LAUNCH_MAX];
            int n = launcher_build(items, LAUNCH_MAX);
            int row = launcher_hit(mx, my, n);
            launcher_open = 0;
            if (row >= 0) {
                if (items[row].is_pkg) launcher_run_pkg(items[row].pkg_idx);
                else launcher_open_wintype(items[row].wintype);
            }
        } else if (left_now && !prev_left) {

            int hit = -1;
            for (int i=zcount-1;i>=0 && hit<0;i--) {
                int t = zorder[i];
                winrec_t *w = &wins[t];
                if (w->active && !w->minimized && point_in(mx,my,w->x,w->y,w->w,w->h)) hit = t;
            }

            if (hit >= 0) {
                winrec_t *w = &wins[hit];
                wm_push_front(hit);
                if (my < w->y+11) {
                    if (mx>=w->x+w->w-11 && mx<w->x+w->w-2) {
                        wm_close(hit);
                    } else if (mx>=w->x+w->w-22 && mx<w->x+w->w-13) {
                        w->minimized = 1;
                    } else {
                        dragging = hit; drag_ox = mx-w->x; drag_oy = my-w->y;
                    }
                } else if (mx>=w->x+w->w-8 && mx<w->x+w->w && my>=w->y+w->h-8 && my<w->y+w->h) {
                    resizing = hit; resize_ow = w->w; resize_oh = w->h;
                    resize_mx = mx; resize_my = my;
                } else if (hit == WIN_FILES) {
                    files_handle_click(w, mx, my);
                } else if (hit == WIN_CALC) {
                    calc_handle_click(w, mx, my);
                } else if (hit == WIN_EDITOR) {
                    editor_handle_click(w, mx, my);
                } else if (hit == WIN_PKG) {
                    pkg_handle_click(w, mx, my);
                }
            } else if (my < 10 && mx < 33) {
                launcher_open = 1;
            } else if (my < 10 && mx >= tb_cascade_x0 && mx < tb_cascade_x1) {
                wm_cascade();
            } else if (my < 10 && mx >= tb_tile_x0 && mx < tb_tile_x1) {
                wm_tile();
            } else if (my < 10) {
                int th = taskbar_hit(mx, my);
                if (th >= 0) { wins[th].minimized = 0; wm_push_front(th); }
            } else if (mx < 48 && my >= ICON_START_Y) {
                int iconidx = (my-ICON_START_Y)/ICON_SLOT;
                if (iconidx==0)      wm_open(WIN_TERMINAL, 40,  20, TERM_W, TERM_H);
                else if (iconidx==1) wm_open(WIN_FILES,   760,  20, 230, 290);
                else if (iconidx==2) wm_open(WIN_SYSMON,  760, 330, 230, 290);
                else if (iconidx==3) wm_open(WIN_CALC,    480, 500, 190, 230);
                else if (iconidx==4) wm_open(WIN_EDITOR,   40, 500, 420, 230);
                else if (iconidx==5) wm_open(WIN_PKG,     690, 500, 300, 230);
                else if (iconidx==6) running = 0;
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

        if (left_now && resizing >= 0) {
            winrec_t *w = &wins[resizing];
            int nw = resize_ow + (mx - resize_mx);
            int nh = resize_oh + (my - resize_my);
            if (nw < w->min_w) nw = w->min_w;
            if (nh < w->min_h) nh = w->min_h;
            if (w->x+nw > GUI_WIDTH)  nw = GUI_WIDTH  - w->x;
            if (w->y+nh > GUI_HEIGHT) nh = GUI_HEIGHT - w->y;
            w->w = nw; w->h = nh;
        }
        if (!left_now) resizing = -1;

        prev_left = left_now;

        __asm__ volatile("hlt");
    }

    gui_exit();
}
