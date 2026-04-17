
#include <stdint.h>
#include <stddef.h>
#include "vga.h"
#include "keyboard.h"
#include "kstring.h"
#include "kmalloc.h"
#include "process.h"
#include "fs.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "sched.h"
#include "paging.h"
#include "ata.h"
#include "fat12.h"
#include "syscall.h"
#include "userspace.h"
#include "elf.h"
#include "serial.h"
#include "rtc.h"
#include "mouse.h"
#include "gui.h"
#include "vfs.h"
#include "pipe.h"
#include "net.h"
#include "signal.h"
#include "procfs.h"

#define MULTIBOOT_MAGIC  0x2BADB002
#define MULTIBOOT_FLAG_MEM (1<<0)

typedef struct {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr, syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr;
    uint32_t config_table, boot_loader_name;
} multiboot_info_t;

#define CMD_LEN   256
#define HIST_SIZE  16
static char history[HIST_SIZE][CMD_LEN];
static int  hist_count = 0;
static int  kum_active = 0;
static char hostname[32] = "kumos";
uint32_t total_mem_kb = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v;
}

static void kprintf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if      (*fmt == 's') { char *s = __builtin_va_arg(ap,char*); vga_puts(s?s:"(null)"); }
            else if (*fmt == 'd') { int d=__builtin_va_arg(ap,int); if(d<0){vga_putchar('-');d=-d;} vga_put_dec((uint32_t)d); }
            else if (*fmt == 'u') { vga_put_dec(__builtin_va_arg(ap,uint32_t)); }
            else if (*fmt == 'x') { vga_put_hex(__builtin_va_arg(ap,uint32_t)); }
            else if (*fmt == 'c') { vga_putchar((char)__builtin_va_arg(ap,int)); }
            else if (*fmt == '%') { vga_putchar('%'); }
        } else { vga_putchar(*fmt); }
        fmt++;
    }
    __builtin_va_end(ap);
}

static void draw_statusbar(void) {
    vga_fill_rect(0,0,80,1,' ',VGA_BLACK,VGA_CYAN);
    vga_puts_at(" KumOS v1.1",0,0,VGA_BLACK,VGA_CYAN);

    char ubuf[20]="UP:"; char num[12];
    kitoa(timer_seconds(),num,10); kstrcat(ubuf,num); kstrcat(ubuf,"s");
    vga_puts_at(ubuf,28,0,VGA_BLACK,VGA_CYAN);

    task_t *t = sched_current();
    char tbuf[40]="["; kstrcat(tbuf,t->name); kstrcat(tbuf,"]");
    vga_puts_at(tbuf,44,0,VGA_BLACK,VGA_CYAN);

    vga_puts_at("MEM:",60,0,VGA_BLACK,VGA_CYAN);
    char mb[10]; kitoa(kmalloc_used()/1024,mb,10); kstrcat(mb,"KB");
    vga_puts_at(mb,64,0,VGA_BLACK,VGA_CYAN);
    vga_puts_at(kum_active?"KUM":"   ",76,0,kum_active?VGA_RED:VGA_DARK_GREY,VGA_CYAN);
}

static void draw_splash(void) {
    vga_clear();
    vga_fill_rect(0,0,80,25,' ',VGA_BLACK,VGA_BLUE);
    vga_fill_rect(10,3,60,19,' ',VGA_WHITE,VGA_BLACK);
    vga_draw_box(10,3,60,19,VGA_CYAN,VGA_BLACK);
    vga_puts_at("  _  ___   _ __  __  ___  ____  ",23,5,VGA_CYAN,VGA_BLACK);
    vga_puts_at(" | |/ / | | |  \\/  |/ _ \\/ ___| ",23,6,VGA_CYAN,VGA_BLACK);
    vga_puts_at(" | ' /| | | | |\\/| | | | \\___ \\ ",23,7,VGA_CYAN,VGA_BLACK);
    vga_puts_at(" | . \\| |_| | |  | | |_| |___) |",23,8,VGA_CYAN,VGA_BLACK);
    vga_puts_at(" |_|\\_\\\\___/|_|  |_|\\___/|____/ ",23,9,VGA_CYAN,VGA_BLACK);
    vga_puts_at("    Minimal x86 OS  v1.2          ",22,11,VGA_YELLOW,VGA_BLACK);
    vga_puts_at("  GDT|IDT|PIT|IRQ|Sched|Paging   ",22,12,VGA_LIGHT_GREY,VGA_BLACK);

    const char *msgs[] = {
        "  [ OK ] GDT  Setting up descriptor tables...",
        "  [ OK ] IDT  Installing interrupt handlers...",
        "  [ OK ] PAG  Paging enabled  (CR0.PG=1, 16MB mapped)...",
        "  [ OK ] PIC  Remapping 8259 controller...",
        "  [ OK ] PIT  Timer at 100 Hz via IRQ0...",
        "  [ OK ] KBD  IRQ1 keyboard handler active...",
        "  [ OK ] SCH  Preemptive scheduler started...",
        "  [ .. ] SHL  Launching kshell...",
    };
    for (int i = 0; i < 8; i++) {
        vga_puts_at(msgs[i], 12, 14+i, i<7?VGA_GREEN:VGA_YELLOW, VGA_BLACK);
        for (volatile int d = 0; d < 2000000; d++);
    }
    for (volatile int d = 0; d < 5000000; d++);
}

static void print_prompt(void) {
    draw_statusbar();
    vga_set_color(kum_active?VGA_RED:VGA_GREEN,VGA_BLACK);
    vga_puts(kum_active?"root":"user");
    vga_set_color(VGA_WHITE,VGA_BLACK); vga_putchar('@');
    vga_set_color(VGA_CYAN,VGA_BLACK);  vga_puts(hostname);
    vga_set_color(VGA_WHITE,VGA_BLACK); vga_putchar(':');
    vga_set_color(VGA_YELLOW,VGA_BLACK);vga_putchar('~');
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts(kum_active?"# ":"$ ");
}

static void hist_add(const char *cmd) {
    if (!*cmd) return;
    if (hist_count < HIST_SIZE) kstrcpy(history[hist_count++], cmd);
    else {
        for (int i=0;i<HIST_SIZE-1;i++) kstrcpy(history[i],history[i+1]);
        kstrcpy(history[HIST_SIZE-1], cmd);
    }
}

static void split_cmd(const char *line, char *cmd, char *rest, int clen) {
    int i=0;
    while(*line&&*line!=' '&&i<clen-1) cmd[i++]=*line++;
    cmd[i]=0; while(*line==' ')line++;
    kstrcpy(rest,line);
}

static int calc_parse(const char *e) {
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

static void cmd_logo(void) {
    vga_set_color(VGA_CYAN,VGA_BLACK);
    vga_puts("\n  ##    ## ##     ## ##     ##  #######   ######  \n");
    vga_puts("  ##   ##  ##     ## ###   ### ##     ## ##    ## \n");
    vga_puts("  ##  ##   ##     ## #### #### ##     ## ##       \n");
    vga_puts("  #####    ##     ## ## ### ## ##     ##  ######  \n");
    vga_puts("  ##  ##   ##     ## ##     ## ##     ##       ## \n");
    vga_puts("  ##   ##  ##     ## ##     ## ##     ## ##    ## \n");
    vga_puts("  ##    ##  #######  ##     ##  #######   ######  \n");
    vga_set_color(VGA_YELLOW,VGA_BLACK);
    vga_puts("       Minimal x86 OS v1.1  |  KumLabs 2025\n\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
}

static void cmd_meminfo(void) {
    vga_set_color(VGA_YELLOW,VGA_BLACK);
    vga_puts("\n  === Memory ===\n\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    kprintf("  Total RAM:       %u KB (%u MB)\n",total_mem_kb,total_mem_kb/1024);
    kprintf("  Phys frames:     %u total  %u used  %u free\n",
            pmm_total(), pmm_used(), pmm_total()-pmm_used());
    kprintf("  Demand faults:   %u handled  %u COW copies\n",
            demand_fault_count(), demand_cow_count());
    vga_puts("\n");
    kprintf("  Static heap:     %u used / %u free  (0x200000-0x27FFFF)\n",
            kmalloc_used(), kmalloc_free());
    kprintf("  Dynamic heap:    %u used  brk=0x%x  cap=%u KB\n",
            heap_used(), heap_brk(), heap_capacity()/1024);
    vga_puts("  vmalloc region:  0x01000000 – 0x02000000  (16 MB)\n");
    vga_puts("  Paging:          ENABLED  (CR0.PG=1)\n\n");
}

static void cmd_vmem(void) {
    vga_set_color(VGA_YELLOW,VGA_BLACK);
    vga_puts("\n  === Virtual Memory Map ===\n\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("  Region              Start        End          Info\n");
    vga_puts("  ------              -----        ---          ----\n");
    vga_puts("  BIOS/IVT            0x00000000   0x000FFFFF   identity\n");
    vga_puts("  VGA framebuf        0x000B8000   0x000BFFFF   identity\n");
    vga_puts("  Kernel              0x00100000   0x001FFFFF   identity R/W\n");
    vga_puts("  Static heap         0x00200000   0x0027FFFF   identity R/W\n");
    vga_puts("  Dynamic heap        0x00280000   0x00FFFFFF   demand-paged\n");
    vga_puts("  vmalloc             0x01000000   0x02000000   on-demand\n");
    vga_puts("  [future user]       0x40000000   0xFFFFFFFF   unmapped\n\n");

    vga_set_color(VGA_CYAN,VGA_BLACK);
    vga_puts("  Live mapped ranges (first 16MB):\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    paging_dump_range(0x00000000, 0x01000000);

    vga_set_color(VGA_YELLOW,VGA_BLACK);
    vga_puts("\n  Demand paging stats:\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    kprintf("    Page faults handled: %u\n", demand_fault_count());
    kprintf("    COW copies made:     %u\n", demand_cow_count());
    kprintf("    PMM frames used:     %u / %u\n", pmm_used(), pmm_total());
    kprintf("    Heap brk:            0x%x  (%u KB used)\n",
            heap_brk(), heap_capacity()/1024);
    vga_putchar('\n');
}

static void cmd_mmap(const char *args) {
    char sub[32], rest2[CMD_LEN];
    split_cmd(args, sub, rest2, 32);

    if (kstrcmp(sub,"alloc")==0) {

        uint32_t kb = 0;
        const char *p = rest2;
        while(*p>='0'&&*p<='9'){kb=kb*10+(*p-'0');p++;}
        if (!kb) kb = 4;
        void *ptr = vmalloc(kb * 1024);
        if (ptr) {
            kprintf("  vmalloc(%u KB) -> 0x%x\n", kb, (uint32_t)ptr);

            *(volatile uint8_t *)ptr = 0xAB;
            kprintf("  First byte written OK (demand page fired if needed)\n");
        } else {
            vga_puts("  vmalloc failed (OOM or region full)\n");
        }

    } else if (kstrcmp(sub,"free")==0) {

        uint32_t addr = 0;
        const char *p = rest2;
        if (p[0]=='0'&&p[1]=='x') p+=2;
        while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')) {
            uint8_t d = *p>='a' ? *p-'a'+10 : *p>='A' ? *p-'A'+10 : *p-'0';
            addr = addr*16+d; p++;
        }
        vmfree((void*)addr);
        kprintf("  vmfree(0x%x) done\n", addr);

    } else if (kstrcmp(sub,"cow")==0) {

        uint32_t addr = 0;
        const char *p = rest2;
        if (p[0]=='0'&&p[1]=='x') p+=2;
        while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')) {
            uint8_t d = *p>='a' ? *p-'a'+10 : *p>='A' ? *p-'A'+10 : *p-'0';
            addr = addr*16+d; p++;
        }
        if (vmalloc_copy_on_write(addr))
            kprintf("  Page 0x%x marked COW (read-only until write)\n", addr);
        else
            vga_puts("  COW failed (page not mapped?)\n");

    } else if (kstrcmp(sub,"query")==0) {

        uint32_t addr = 0;
        const char *p = rest2;
        if (p[0]=='0'&&p[1]=='x') p+=2;
        while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')) {
            uint8_t d = *p>='a' ? *p-'a'+10 : *p>='A' ? *p-'A'+10 : *p-'0';
            addr = addr*16+d; p++;
        }
        if (paging_is_mapped(addr)) {
            uint32_t phys = paging_virt_to_phys(addr);
            kprintf("  0x%x -> phys 0x%x  refcount=%u\n",
                    addr, phys, pmm_refcount(phys));
        } else {
            kprintf("  0x%x is NOT mapped\n", addr);
        }

    } else {
        vga_puts("  Usage:\n");
        vga_puts("    mmap alloc <kb>       - allocate virtual region\n");
        vga_puts("    mmap free <0xaddr>    - free virtual region\n");
        vga_puts("    mmap cow <0xaddr>     - mark page copy-on-write\n");
        vga_puts("    mmap query <0xaddr>   - query mapping + refcount\n");
    }
}

static void cmd_cpuinfo(void) {
    uint32_t eax,ebx,ecx,edx; char vendor[13];
    __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    kmemcpy(vendor+0,&ebx,4); kmemcpy(vendor+4,&edx,4);
    kmemcpy(vendor+8,&ecx,4); vendor[12]=0;
    __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(1));
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("\n  === CPU ===\n\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    kprintf("  Vendor:   %s\n",vendor);
    kprintf("  Family:   %u  Model: %u  Stepping: %u\n",(eax>>8)&0xF,(eax>>4)&0xF,eax&0xF);
    vga_puts("  Features: ");
    if(edx&(1<<0))  vga_puts("FPU ");  if(edx&(1<<4))  vga_puts("TSC ");
    if(edx&(1<<9))  vga_puts("APIC "); if(edx&(1<<23)) vga_puts("MMX ");
    if(edx&(1<<25)) vga_puts("SSE ");  if(edx&(1<<26)) vga_puts("SSE2 ");
    if(ecx&(1<<0))  vga_puts("SSE3 "); if(ecx&(1<<5))  vga_puts("VMX ");
    vga_puts("\n\n");
}

static void cmd_irqinfo(void) {
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("\n  === Interrupt System ===\n\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("  GDT:  6 segments (null/kcode/kdata/ucode/udata/tss)\n");
    vga_puts("  IDT:  256 entries  (INT 0-31=exceptions  32-47=IRQs)\n");
    vga_puts("  PIC:  8259A  Master=INT32-39  Slave=INT40-47\n");
    vga_puts("  IRQ0: PIT timer  100 Hz   ticks=");
    vga_put_dec(timer_ticks()); vga_putchar('\n');
    vga_puts("  IRQ1: PS/2 keyboard  interrupt-driven  buffer=256B\n");
    vga_puts("  Sched: preemptive round-robin  quantum=10 ticks\n\n");
}

static void snake_game(void) {
#define SW 40
#define SH 18
#define SXO 20
#define SYO 4
#define SMAX 200
    vga_clear(); draw_statusbar();
    vga_fill_rect(SXO-1,SYO-1,SW+2,SH+2,' ',VGA_BLACK,VGA_DARK_GREY);
    vga_fill_rect(SXO,SYO,SW,SH,' ',VGA_BLACK,VGA_BLACK);
    vga_puts_at("SNAKE - WASD to move, Q to quit",24,1,VGA_YELLOW,VGA_BLACK);
    vga_puts_at("Score: 0",24,2,VGA_WHITE,VGA_BLACK);

    int sx[SMAX],sy[SMAX],len=3,dir=3,running=1,score=0;
    int fx=30,fy=10;
    for(int i=0;i<len;i++){sx[i]=SXO+SW/2-i;sy[i]=SYO+SH/2;}
    vga_puts_at("*",fx,fy,VGA_RED,VGA_BLACK);

    while(running){
        char c=keyboard_getchar();
        if(c=='w'&&dir!=1)dir=0; if(c=='s'&&dir!=0)dir=1;
        if(c=='a'&&dir!=3)dir=2; if(c=='d'&&dir!=2)dir=3;
        if(c=='q')running=0;

        int nx=sx[0],ny=sy[0];
        if(dir==0)ny--; if(dir==1)ny++;
        if(dir==2)nx--; if(dir==3)nx++;

        if(nx<SXO||nx>=SXO+SW||ny<SYO||ny>=SYO+SH) break;
        for(int i=0;i<len;i++) if(sx[i]==nx&&sy[i]==ny){running=0;break;}
        if(!running) break;

        char blank[2]={' ',0};
        vga_puts_at(blank,sx[len-1],sy[len-1],VGA_BLACK,VGA_BLACK);
        for(int i=len-1;i>0;i--){sx[i]=sx[i-1];sy[i]=sy[i-1];}
        sx[0]=nx; sy[0]=ny;
        for(int i=0;i<len;i++)
            vga_puts_at(i==0?"O":"o",sx[i],sy[i],i==0?VGA_GREEN:VGA_LIGHT_GREEN,VGA_BLACK);

        if(nx==fx&&ny==fy){
            score++; if(len<SMAX)len++;
            fx=SXO+((fx*7+3)%SW); fy=SYO+((fy*13+5)%SH);
            vga_puts_at("*",fx,fy,VGA_RED,VGA_BLACK);
            char sb[20]="Score: "; char sn[10]; kitoa(score,sn,10);
            kstrcat(sb,sn); kstrcat(sb,"   ");
            vga_puts_at(sb,24,2,VGA_WHITE,VGA_BLACK);
        }
        timer_sleep(80);
    }
    vga_puts_at("GAME OVER! Press any key...",27,23,VGA_RED,VGA_BLACK);
    keyboard_getchar_blocking();
}

static void cmd_disk(void) {
    ata_print_info();
    fat12_info();
}

static void cmd_dls(void) {
    if (!fat12_mounted()) {
        vga_puts("  No disk mounted. Run 'dformat' first or attach a disk.\n");
        return;
    }
    fat12_entry_t entries[64];
    int n = fat12_list(entries, 64);
    if (n <= 0) { vga_puts("  (empty)\n"); return; }
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n  NAME             SIZE\n");
    vga_puts("  ----             ----\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (int i = 0; i < n; i++) {
        vga_puts("  ");
        vga_puts(entries[i].name);
        int pad = 17 - (int)kstrlen(entries[i].name);
        for (int p = 0; p < pad; p++) vga_putchar(' ');
        vga_put_dec(entries[i].size);
        vga_puts(" B\n");
    }
    vga_putchar('\n');
}

static void cmd_dcat(const char *name) {
    if (!*name) { vga_puts("Usage: dcat <filename>\n"); return; }
    if (!fat12_mounted()) { vga_puts("No disk mounted.\n"); return; }
    static uint8_t fbuf[4096];
    int n = fat12_read(name, fbuf, sizeof(fbuf)-1);
    if (n < 0) { kprintf("dcat: '%s' not found on disk.\n", name); return; }
    fbuf[n] = 0;
    vga_putchar('\n');
    vga_puts((char *)fbuf);
    vga_putchar('\n');
}

static void cmd_dwrite(const char *args) {
    char fname[32], data[CMD_LEN];
    split_cmd(args, fname, data, 32);
    if (!*fname || !*data) {
        vga_puts("Usage: dwrite <filename> <content>\n"); return;
    }
    if (!fat12_mounted()) { vga_puts("No disk mounted.\n"); return; }
    if (fat12_write(fname, data, kstrlen(data)) == 0)
        kprintf("Written '%s' (%u bytes) to disk.\n", fname, kstrlen(data));
    else
        vga_puts("dwrite: failed (disk full or I/O error)\n");
}

static void cmd_drm(const char *name) {
    if (!*name) { vga_puts("Usage: drm <filename>\n"); return; }
    if (!fat12_mounted()) { vga_puts("No disk mounted.\n"); return; }
    if (!kum_active) { vga_puts("Permission denied. Use 'kum drm <file>'\n"); return; }
    if (fat12_delete(name) == 0)
        kprintf("Deleted '%s' from disk.\n", name);
    else
        kprintf("drm: '%s' not found.\n", name);
}

static void cmd_dformat(void) {
    if (!kum_active) {
        vga_puts("Permission denied. Use 'kum dformat'\n"); return;
    }
    if (ata_get(0) == 0) {
        vga_puts("dformat: no ATA drive found.\n"); return;
    }
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Formatting drive 0 with FAT12 (label: KUMOS)...\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    if (fat12_format(0, "KUMOS") == 0) {
        vga_puts("Format complete. Drive mounted as FAT12.\n");

        const char *welcome =
            "Welcome to KumOS!\n"
            "This file lives on your ATA disk and survives reboots.\n"
            "Use dls, dcat, dwrite, drm to manage disk files.\n";
        fat12_write("README.TXT", welcome, kstrlen(welcome));
        vga_puts("Created README.TXT on disk.\n");
    } else {
        vga_puts("dformat: failed.\n");
    }
}

static void cmd_dcp(const char *args) {

    char src[32], dst[32];
    split_cmd(args, src, dst, 32);
    if (!*src || !*dst) { vga_puts("Usage: dcp <diskfile> <memfile>\n"); return; }
    if (!fat12_mounted()) { vga_puts("No disk mounted.\n"); return; }
    static uint8_t cbuf[4096];
    int n = fat12_read(src, cbuf, sizeof(cbuf)-1);
    if (n < 0) { kprintf("dcp: '%s' not found on disk.\n", src); return; }
    cbuf[n] = 0;
    fs_write(dst, (char *)cbuf);
    kprintf("Copied disk:'%s' -> mem:'%s' (%d bytes)\n", src, dst, n);
}
static void dispatch(const char *line);

static void cmd_help(void) {
    vga_set_color(VGA_CYAN,VGA_BLACK);
    vga_puts("\n === KumOS Shell v1.1 ===\n\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  General:\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    help, clear, echo, uname, whoami, hostname, uptime, history\n");
    vga_puts("    date              - Current date/time (RTC)\n");
    vga_puts("    mouse             - PS/2 mouse state\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  Files:\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    ls, cat <f>, touch <f>, write <f> <data>, rm <f>\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  System:\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    ps       - Task list (real scheduler)\n");
    vga_puts("    meminfo  - RAM, frames, heap, demand-paging stats\n");
    vga_puts("    vmem     - Virtual memory map + live page table walk\n");
    vga_puts("    mmap     - Map/unmap/COW/query virtual pages\n");
    vga_puts("    cpuinfo  - CPU via CPUID\n");
    vga_puts("    irqinfo  - GDT/IDT/PIC/PIT/Sched live status\n");
    vga_puts("    ifconfig          - NIC info + IP address\n");
    vga_puts("    ping              - Send UDP to gateway\n");
    vga_puts("    netrecv [port]    - Listen for UDP packet\n");
    vga_puts("    kill <pid> [sig]  - Send signal to process\n");
    vga_puts("    proc [file]       - Read /proc/file (meminfo,ps,uptime...)\n");
    vga_puts("    serial   - COM1 status + test message\n");
    vga_puts("    gui      - Launch graphical desktop (Mode 13h)\n");
    vga_puts("    hexdump <addr> [len] - memory hex dump\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  Disk (FAT12 on ATA):\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    disk              - ATA drive info + FAT12 volume\n");
    vga_puts("    dls               - List files on disk\n");
    vga_puts("    dcat <f>          - Print file from disk\n");
    vga_puts("    dwrite <f> <data> - Write file to disk\n");
    vga_puts("    drm <f>           - Delete file from disk (kum)\n");
    vga_puts("    dformat           - Format disk FAT12 (kum)\n");
    vga_puts("    dcp <disk> <mem>  - Copy disk file to memory fs\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  Userspace (ring 3):\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    run               - List user programs\n");
    vga_puts("    run hello         - Hello world from ring 3\n");
    vga_puts("    run counter       - Count 0-9 with sleep\n");
    vga_puts("    run top           - Live uptime (5 passes)\n");
    vga_puts("    run echo          - Read + echo a line\n");
    vga_puts("    run cat           - Read README from disk\n");
    vga_puts("    reboot   - Reboot (kum required)\n");
    vga_puts("    shutdown - Halt   (kum required)\n");
    vga_puts("    wait <pid>        - Wait for process to exit\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  KUM:\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    kum              - Enter privileged session\n");
    vga_puts("    kum exit         - Drop privileges\n");
    vga_puts("    kum <cmd>        - Run one command elevated\n");
    vga_puts("    kum kill <pid>   - Kill a task\n");
    vga_puts("    kum spawn <name> - Spawn background task\n");
    vga_puts("    kum sethost <n>  - Change hostname\n");
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("  Fun:\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    vga_puts("    snake, calc <expr>, banner <text>, logo, motd\n\n");
}

static void cmd_kum(const char *args) {
    while(*args==' ')args++;
    if(!*args){ kum_active=1; vga_set_color(VGA_YELLOW,VGA_BLACK);
        vga_puts("[kum] Privileged session. 'kum exit' to leave.\n");
        vga_set_color(VGA_WHITE,VGA_BLACK); return; }
    if(kstrcmp(args,"exit")==0){ kum_active=0;
        vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("[kum] Dropped privileges.\n");
        vga_set_color(VGA_WHITE,VGA_BLACK); return; }
    if(kstartswith(args,"spawn ")){
        int pid=sched_spawn(args+6,0,1);
        kprintf("[kum] Background task '%s' spawned  PID=%d\n",args+6,pid); return; }
    if(kstartswith(args,"kill ")){
        const char *p=args+5; int pid=0;
        while(*p>='0'&&*p<='9'){pid=pid*10+(*p-'0');p++;}
        if(pid<=1){vga_puts("[kum] Cannot kill kernel tasks.\n");return;}
        proc_kill(pid); kprintf("[kum] Killed PID %d\n",pid); return; }
    if(kstartswith(args,"sethost ")){
        kstrcpy(hostname,args+8);
        kprintf("[kum] Hostname set to '%s'\n",hostname); return; }
    int prev=kum_active; kum_active=1;
    vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("[kum] ");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    dispatch(args); kum_active=prev;
}

static void dispatch(const char *line) {
    while(*line==' ')line++; if(!*line)return;
    char cmd[64],rest[CMD_LEN];
    split_cmd(line,cmd,rest,64);

    if(kstrcmp(cmd,"help")==0){ cmd_help(); }
    else if(kstrcmp(cmd,"clear")==0){ vga_clear(); draw_statusbar(); }
    else if(kstrcmp(cmd,"logo")==0){ cmd_logo(); }
    else if(kstrcmp(cmd,"uname")==0){ vga_puts("KumOS 1.4 kumos i686 KumOS/KumLabs\n"); }
    else if(kstrcmp(cmd,"whoami")==0){ vga_puts(kum_active?"root (kum)\n":"user\n"); }
    else if(kstrcmp(cmd,"date")==0) {
        vga_puts("  ");
        rtc_print();
        vga_putchar('\n');
    }
    else if(kstrcmp(cmd,"mouse")==0) {
        mouse_state_t *m = mouse_get();
        if (!mouse_ready()) {
            vga_puts("  Mouse not detected.\n");
        } else {
            vga_set_color(VGA_YELLOW,VGA_BLACK);
            vga_puts("\n  === Mouse State ===\n\n");
            vga_set_color(VGA_WHITE,VGA_BLACK);
            kprintf("  Position: x=%d  y=%d\n", m->x, m->y);
            kprintf("  Buttons:  L=%d  R=%d  M=%d\n",
                    m->left, m->right, m->middle);
            kprintf("  Last delta: dx=%d  dy=%d\n\n", m->dx, m->dy);
        }
    }
    else if(kstrcmp(cmd,"wait")==0) {
        if (!*rest) { vga_puts("Usage: wait <pid>\n"); }
        else {
            int pid = 0;
            const char *p = rest;
            while(*p>='0'&&*p<='9'){pid=pid*10+(*p-'0');p++;}
            kprintf("Waiting for PID %d...\n", pid);
            int code = sched_waitpid(pid);
            kprintf("PID %d exited with code %d\n", pid, code);
        }
    }
    else if(kstrcmp(cmd,"hostname")==0){ vga_puts(hostname); vga_putchar('\n'); }
    else if(kstrcmp(cmd,"uptime")==0){
        uint32_t s=timer_seconds(),m=s/60; s%=60; uint32_t h=m/60; m%=60;
        vga_puts("  uptime: ");
        if(h){vga_put_dec(h);vga_puts("h ");}
        if(m){vga_put_dec(m);vga_puts("m ");}
        vga_put_dec(s); vga_puts("s  |  ticks: "); vga_put_dec(timer_ticks());
        vga_puts("  |  100 Hz PIT\n"); }
    else if(kstrcmp(cmd,"echo")==0){ vga_puts(rest); vga_putchar('\n'); }
    else if(kstrcmp(cmd,"ls")==0){ vga_putchar('\n'); fs_list(); vga_putchar('\n'); }
    else if(kstrcmp(cmd,"cat")==0){
        if(!*rest){vga_puts("Usage: cat <file>\n");return;}
        fs_file_t *f=fs_find(rest);
        if(!f){kprintf("cat: %s: No such file\n",rest);return;}
        vga_putchar('\n'); vga_puts(f->data); vga_putchar('\n'); }
    else if(kstrcmp(cmd,"touch")==0){
        if(!*rest){vga_puts("Usage: touch <file>\n");return;}
        if(fs_create(rest,"",0)==0) kprintf("Created '%s'\n",rest);
        else kprintf("touch: '%s' exists or no space\n",rest); }
    else if(kstrcmp(cmd,"write")==0){
        char fn[64],dt[CMD_LEN]; split_cmd(rest,fn,dt,64);
        if(!*fn){vga_puts("Usage: write <file> <data>\n");return;}
        fs_write(fn,dt); kprintf("Written '%s'\n",fn); }
    else if(kstrcmp(cmd,"rm")==0){
        if(!*rest){vga_puts("Usage: rm <file>\n");return;}
        if(!kum_active){vga_puts("Permission denied. Use 'kum rm <file>'\n");return;}
        if(fs_delete(rest)==0) kprintf("Deleted '%s'\n",rest);
        else kprintf("rm: '%s': Not found\n",rest); }
    else if(kstrcmp(cmd,"ps")==0){ vga_putchar('\n'); sched_list(); vga_putchar('\n'); }
    else if(kstrcmp(cmd,"meminfo")==0){ cmd_meminfo(); }
    else if(kstrcmp(cmd,"vmem")==0){ cmd_vmem(); }
    else if(kstrcmp(cmd,"mmap")==0){ cmd_mmap(rest); }
    else if(kstrcmp(cmd,"cpuinfo")==0){ cmd_cpuinfo(); }
    else if(kstrcmp(cmd,"irqinfo")==0){ cmd_irqinfo(); }
    else if(kstrcmp(cmd,"ifconfig")==0) {
        net_print_info();
    }
    else if(kstrcmp(cmd,"ping")==0) {
        if (!net_ready()) { vga_puts("No NIC.\n"); }
        else {
            vga_puts("Sending UDP ping to 10.0.2.2:7 (echo port)...\n");
            const char *msg = "KumOS ping";
            int r = net_send_udp(NET_IP(10,0,2,2), 1234, 7, msg, 10);
            kprintf("  Result: %s\n", r==0?"sent OK":"send failed");
        }
    }
    else if(kstrcmp(cmd,"netrecv")==0) {
        if (!net_ready()) { vga_puts("No NIC.\n"); }
        else {
            uint16_t port = 1234;
            if (*rest) { port=0; const char *p=rest; while(*p>='0'&&*p<='9'){port=(uint16_t)(port*10+(*p-'0'));p++;} }
            kprintf("Listening on UDP port %u (polling 2s)...\n", port);
            uint8_t buf[256]; uint32_t src_ip=0; uint16_t src_port=0;
            uint32_t end = timer_ticks() + 200;
            int got = 0;
            while (timer_ticks() < end) {
                net_poll();
                int n = net_recv_udp(port, buf, 255, &src_ip, &src_port);
                if (n > 0) {
                    buf[n]=0;
                    kprintf("  From %u.%u.%u.%u:%u  \"%s\"\n",
                        (src_ip>>24)&0xFF,(src_ip>>16)&0xFF,(src_ip>>8)&0xFF,src_ip&0xFF,
                        src_port, buf);
                    got=1; break;
                }
            }
            if (!got) vga_puts("  No packets received.\n");
        }
    }
    else if(kstrcmp(cmd,"proc")==0) {
        const char *file = *rest ? rest : "meminfo";
        char path[64]; kstrcpy(path,"/proc/"); kstrcat(path,file);
        int fd = vfs_open(path, 0);
        if (fd < 0) { kprintf("proc: /proc/%s not found\n", file);
                      vga_puts("  Files: meminfo uptime version ps net date\n"); }
        else {
            vga_putchar('\n');
            char buf[512]; int n;
            while ((n=vfs_read(fd,buf,511))>0) { buf[n]=0; vga_puts(buf); }
            vfs_close(fd);
            vga_putchar('\n');
        }
    }
    else if(kstrcmp(cmd,"kill")==0) {
        char arg1[16], arg2[16];
        split_cmd(rest, arg1, arg2, 16);
        int sig=15, pid=0;
        if (*arg2) { const char *p=arg1; while(*p>='0'&&*p<='9'){pid=pid*10+(*p-'0');p++;} sig=0; const char *q=arg2; while(*q>='0'&&*q<='9'){sig=sig*10+(*q-'0');q++;} }
        else { const char *p=arg1; while(*p>='0'&&*p<='9'){pid=pid*10+(*p-'0');p++;} }
        if (!pid) { vga_puts("Usage: kill <pid> [signal]\n"); }
        else { signal_send(pid, sig); kprintf("Sent signal %d to PID %d\n", sig, pid); }
    }
    else if(kstrcmp(cmd,"kush")==0) {

        vga_set_color(VGA_CYAN,VGA_BLACK);
        vga_puts("Launching kush (userspace shell)...\n");
        vga_set_color(VGA_WHITE,VGA_BLACK);
        elf_load_result_t r = elf_load_disk("KUSH.ELF");
        if (r.error == 0) {
            int pid = elf_spawn("kush", &r);
            if (pid >= 0) {
                int code = sched_waitpid(pid);
                kprintf("\n[kush exited with code %d]\n", code);
            } else vga_puts("kush: spawn failed\n");
        } else {
            vga_puts("kush: KUSH.ELF not found on disk.\n");
            vga_puts("Build it with: make user-programs\n");
        }
    }
    else if(kstrcmp(cmd,"gui")==0) {
        vga_set_color(VGA_CYAN,VGA_BLACK);
        vga_puts("Switching to GUI mode (320x200)...\n");
        vga_puts("Press ESC inside GUI to return.\n");
        vga_set_color(VGA_WHITE,VGA_BLACK);
        timer_sleep(500);
        gui_run();

        vga_clear();
        draw_statusbar();
        vga_set_color(VGA_GREEN,VGA_BLACK);
        vga_puts("  Returned from GUI.\n\n");
        vga_set_color(VGA_WHITE,VGA_BLACK);
    }
    else if(kstrcmp(cmd,"serial")==0) {
        vga_set_color(VGA_YELLOW,VGA_BLACK);
        vga_puts("\n  === Serial Port (COM1) ===\n\n");
        vga_set_color(VGA_WHITE,VGA_BLACK);
        vga_puts("  Port:    0x3F8 (COM1)\n");
        vga_puts("  Baud:    115200\n");
        vga_puts("  Format:  8N1\n");
        vga_puts("  Status:  ");
        if (serial_ready()) {
            vga_set_color(VGA_GREEN,VGA_BLACK);
            vga_puts("ONLINE\n");
            vga_set_color(VGA_WHITE,VGA_BLACK);
            vga_puts("\n  QEMU:  -serial stdio        (live terminal)\n");
            vga_puts("         -serial file:k.log   (capture to file)\n\n");

            serial_printf("\r\n[serial] Test message from KumOS shell at uptime=%us\r\n",
                         timer_seconds());
            vga_puts("  Test message sent to serial.\n\n");
        } else {
            vga_set_color(VGA_DARK_GREY,VGA_BLACK);
            vga_puts("NOT DETECTED\n\n");
            vga_set_color(VGA_WHITE,VGA_BLACK);
        }
    }
    else if(kstrcmp(cmd,"hexdump")==0) {

        if (!*rest) { vga_puts("Usage: hexdump <0xaddr> [len]\n"); }
        else {
            uint32_t addr = 0; uint32_t len = 64;
            const char *p = rest;
            if (p[0]=='0'&&p[1]=='x') p+=2;
            while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')) {
                uint8_t d=*p>='a'?*p-'a'+10:*p>='A'?*p-'A'+10:*p-'0';
                addr=addr*16+d; p++;
            }
            while(*p==' ')p++;
            if(*p) { len=0; while(*p>='0'&&*p<='9'){len=len*10+(*p-'0');p++;} }
            if(len>512)len=512;
            if(!paging_is_mapped(addr)){
                kprintf("hexdump: 0x%x is not mapped\n",addr);
            } else {
                vga_set_color(VGA_CYAN,VGA_BLACK);
                kprintf("\n  hexdump 0x%x  len=%u\n\n", addr, len);
                vga_set_color(VGA_WHITE,VGA_BLACK);
                serial_hexdump(COM1, (void*)addr, len);

                const uint8_t *bp = (const uint8_t*)addr;
                for(uint32_t i=0;i<len;i+=16){
                    vga_put_hex(addr+i); vga_puts(": ");
                    for(uint32_t j=0;j<16&&i+j<len;j++){
                        char hx[3]; const char *h="0123456789ABCDEF";
                        hx[0]=h[bp[i+j]>>4]; hx[1]=h[bp[i+j]&0xF]; hx[2]=0;
                        vga_puts(hx); vga_putchar(' ');
                    }
                    vga_putchar('\n');
                }
                vga_putchar('\n');
            }
        }
    }
    else if(kstrcmp(cmd,"disk")==0)  { cmd_disk(); }
    else if(kstrcmp(cmd,"dls")==0)   { cmd_dls(); }
    else if(kstrcmp(cmd,"dcat")==0)  { cmd_dcat(rest); }
    else if(kstrcmp(cmd,"dwrite")==0){ cmd_dwrite(rest); }
    else if(kstrcmp(cmd,"drm")==0)   { cmd_drm(rest); }
    else if(kstrcmp(cmd,"dformat")==0){ cmd_dformat(); }
    else if(kstrcmp(cmd,"dcp")==0)   { cmd_dcp(rest); }
    else if(kstrcmp(cmd,"run")==0) {
        if (!*rest) {
            vga_set_color(VGA_YELLOW,VGA_BLACK);
            vga_puts("  Available programs:\n");
            vga_set_color(VGA_WHITE,VGA_BLACK);
            for (int i = 0; i < uprog_count; i++) {
                vga_puts("    run "); vga_puts(uprog_list[i].name); vga_putchar('\n');
            }
            vga_putchar('\n');
        } else {
            int found = 0;
            for (int i = 0; i < uprog_count; i++) {
                if (kstrcmp(rest, uprog_list[i].name) == 0) {
                    found = 1;
                    int pid = user_spawn(rest, uprog_list[i].entry);
                    if (pid < 0) {
                        vga_puts("run: failed to spawn process\n");
                    } else {
                        kprintf("  Spawned '%s' as ring-3 process  PID=%d\n", rest, pid);
                        int code = sched_waitpid(pid);
                        kprintf("  Process exited with code %d\n", code);
                    }
                    break;
                }
            }
            if (!found) kprintf("run: unknown program '%s' — type 'run' to list\n", rest);
        }
    }
    else if(kstrcmp(cmd,"history")==0){
        vga_puts("\n  Command history:\n");
        for(int i=0;i<hist_count;i++){vga_puts("  ");vga_put_dec(i+1);vga_puts(". ");vga_puts(history[i]);vga_putchar('\n');}
        vga_putchar('\n'); }
    else if(kstrcmp(cmd,"calc")==0){
        if(!*rest){vga_puts("Usage: calc <expr>  e.g. calc 6 * 7\n");return;}
        kprintf("  = %d\n",calc_parse(rest)); }
    else if(kstrcmp(cmd,"banner")==0){
        if(!*rest){vga_puts("Usage: banner <text>\n");return;}
        vga_set_color(VGA_YELLOW,VGA_BLACK); vga_puts("\n");
        const char *p=rest; while(*p&&p-rest<8){vga_putchar(*p);vga_putchar(*p);vga_putchar(*p);vga_putchar(' ');p++;}
        vga_putchar('\n');
        p=rest; while(*p&&p-rest<8){vga_putchar(*p);vga_putchar(*p);vga_putchar(*p);vga_putchar(' ');p++;}
        vga_puts("\n"); vga_set_color(VGA_WHITE,VGA_BLACK); }
    else if(kstrcmp(cmd,"snake")==0){ snake_game(); vga_clear(); draw_statusbar(); }
    else if(kstrcmp(cmd,"motd")==0){
        fs_file_t *f=fs_find("motd.txt");
        if(f){vga_set_color(VGA_CYAN,VGA_BLACK);vga_puts(f->data);vga_set_color(VGA_WHITE,VGA_BLACK);} }
    else if(kstrcmp(cmd,"kum")==0){ cmd_kum(rest); }
    else if(kstrcmp(cmd,"reboot")==0){
        if(!kum_active){vga_puts("Permission denied. Use 'kum reboot'\n");return;}
        vga_puts("Rebooting...\n"); timer_sleep(500);
        __asm__ volatile("lidt 0\n\t int $0x01"); }
    else if(kstrcmp(cmd,"shutdown")==0||kstrcmp(cmd,"halt")==0){
        if(!kum_active){vga_puts("Permission denied. Use 'kum shutdown'\n");return;}
        vga_set_color(VGA_YELLOW,VGA_BLACK);
        vga_puts("\nSystem halted. Power off safely.\n");
        __asm__ volatile("cli; hlt"); }
    else if(kstrcmp(cmd,"exec")==0) {
        if (!*rest) { vga_puts("Usage: exec <file.elf>\n"); }
        else {
            vga_set_color(VGA_CYAN,VGA_BLACK);
            kprintf("Loading '%s'...\n", rest);
            vga_set_color(VGA_WHITE,VGA_BLACK);
            elf_load_result_t r = elf_load_disk(rest);
            if (r.error == 0) {
                kprintf("  entry=0x%x  range=0x%x..0x%x\n",r.entry,r.load_base,r.load_end);
                int pid = elf_spawn(rest, &r);
                if (pid >= 0) {
                    kprintf("  Running as ring-3 PID=%d\n", pid);
                    int code = sched_waitpid(pid);
                    kprintf("  Process exited with code %d\n", code);
                } else vga_puts("  Spawn failed.\n");
            } else {
                const char *errs[]={"","bad magic","bad arch","OOM","disk error"};
                kprintf("  Error %d: %s\n", r.error,
                        (r.error>=-4&&r.error<=0)?errs[-r.error]:"?");
            }
        }
    }
    else if(kstrcmp(cmd,"elfinfo")==0) {
        if (!*rest) { vga_puts("Usage: elfinfo <file.elf>\n"); }
        else {
            static uint8_t ebuf[65536];
            int n = fat12_mounted() ? fat12_read(rest, ebuf, sizeof(ebuf)) : -1;
            if (n <= 0) {
                fs_file_t *mf = fs_find(rest);
                if (mf) { n=(int)mf->size; kmemcpy(ebuf,mf->data,(uint32_t)n); }
            }
            if (n > 0) elf_print_info(ebuf, (uint32_t)n);
            else kprintf("elfinfo: '%s' not found\n", rest);
        }
    }
    else {
        kprintf("ksh: command not found: %s\n",cmd);
        vga_puts("Type 'help' for available commands.\n"); }
}

void kernel_main(uint32_t magic, multiboot_info_t *mbi) {

    vga_init();

    serial_init(COM1);
    serial_puts(COM1, "\r\n\r\n");
    serial_puts(COM1, "========================================\r\n");
    serial_puts(COM1, "  KumOS v1.4 — Serial Boot Log (COM1)  \r\n");
    serial_puts(COM1, "========================================\r\n");

    if (magic == MULTIBOOT_MAGIC && mbi && (mbi->flags & MULTIBOOT_FLAG_MEM))
        total_mem_kb = mbi->mem_lower + mbi->mem_upper;
    else
        total_mem_kb = 32768;
    serial_printf("[boot] RAM: %u KB (%u MB)\r\n", total_mem_kb, total_mem_kb/1024);

    kmalloc_init(0x00200000, 512*1024);
    serial_printf("[boot] Heap @ 0x200000, 512KB\r\n");

    gdt_init();
    serial_printf("[boot] GDT loaded (6 descriptors)\r\n");

    idt_init();
    serial_printf("[boot] IDT loaded (256 entries), PIC remapped\r\n");

    paging_init(total_mem_kb);
    serial_printf("[boot] Paging enabled, %u frames total\r\n", pmm_total());

    demand_paging_init();
    serial_printf("[boot] Demand paging active (INT14 hooked)\r\n");

    timer_init(100);
    serial_printf("[boot] PIT timer @ 100Hz (IRQ0)\r\n");

    keyboard_init();
    serial_printf("[boot] Keyboard driver active (IRQ1)\r\n");

    proc_init();
    fs_init();
    serial_printf("[boot] In-memory FS ready\r\n");

    ata_init();
    fat12_mount(0);
    if (fat12_mounted())
        serial_printf("[boot] FAT12 mounted on ATA drive 0\r\n");
    else
        serial_printf("[boot] No FAT12 disk found\r\n");

    vfs_init();
    vfs_init_stdio();
    proc_fs_init();
    serial_printf("[boot] VFS ready (/mem /disk /dev /proc)\r\n");

    signal_init();
    serial_printf("[boot] Signal subsystem ready\r\n");

    sched_init();
    serial_printf("[boot] Scheduler ready\r\n");

    syscall_init();
    serial_printf("[boot] Syscall interface ready (INT 0x80)\r\n");

    rtc_init();
    mouse_init();
    serial_printf("[boot] RTC ready  Mouse: %s\r\n",
                  mouse_ready() ? "detected" : "not found");

    if (net_init() == 0)
        serial_printf("[boot] RTL8139 NIC ready  IP: 10.0.2.15\r\n");
    else
        serial_printf("[boot] No NIC found\r\n");

    serial_printf("[boot] Enabling interrupts...\r\n");
    __asm__ volatile ("sti");

    draw_splash();
    vga_clear();
    draw_statusbar();

    vga_set_color(VGA_CYAN,VGA_BLACK);
    fs_file_t *motd = fs_find("motd.txt");
    if (motd) vga_puts(motd->data);
    vga_set_color(VGA_LIGHT_GREY,VGA_BLACK);
    kprintf("  KumOS v1.8  |  RAM: %u KB  |  VFS+Net+Signals+kush\n",total_mem_kb);
    serial_printf("[boot] Shell started. Type commands below.\r\n");
    serial_printf("----------------------------------------\r\n");
    vga_puts("  Type 'help' for commands. 'irqinfo' to see interrupt status.\n");
    vga_puts("  Use 'kum <cmd>' instead of sudo for privileged ops.\n\n");

    {
        uint8_t pic_mask = inb(0x21);
        uint8_t kb_stat  = inb(0x64);
        vga_set_color(VGA_DARK_GREY, VGA_BLACK);
        vga_puts("  [kbd] PIC1 mask=0x");
        vga_put_hex(pic_mask);
        vga_puts("  IRQ1=");
        vga_puts((pic_mask & 0x02) ? "MASKED(bad)" : "unmasked(ok)");
        vga_puts("  8042 status=0x");
        vga_put_hex(kb_stat);
        vga_puts("  mode=hybrid(IRQ+poll)\n\n");
    }
    vga_set_color(VGA_WHITE,VGA_BLACK);

    char line[CMD_LEN];
    while (1) {
        print_prompt();
        keyboard_getline(line, CMD_LEN);
        if (!*line) continue;
        hist_add(line);
        serial_printf("[ksh] %s\r\n", line);
        dispatch(line);
    }
}