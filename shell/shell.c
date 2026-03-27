#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "klibc.h"
#include "memory.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_CMD     256
#define MAX_ARGS    16
#define MAX_HISTORY 20
#define MAX_FILES   32
#define MAX_FSIZE   512

/* ── Simple in-memory filesystem ─────────────────────────── */
typedef struct {
    char  name[32];
    char  data[MAX_FSIZE];
    int   size;
    int   used;
} file_t;

static file_t fs[MAX_FILES];
static int    fs_count = 0;

static file_t *fs_find(const char *name) {
    for (int i = 0; i < MAX_FILES; i++)
        if (fs[i].used && kstrcmp(fs[i].name, name) == 0)
            return &fs[i];
    return NULL;
}

static file_t *fs_create(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].used) {
            kmemset(&fs[i], 0, sizeof(file_t));
            kstrncpy(fs[i].name, name, 31);
            fs[i].used = 1;
            fs_count++;
            return &fs[i];
        }
    }
    return NULL;
}

/* ── Command history ──────────────────────────────────────── */
static char history[MAX_HISTORY][MAX_CMD];
static int  hist_count = 0;
static int  hist_idx   = 0;

static void hist_add(const char *cmd) {
    if (hist_count < MAX_HISTORY) {
        kstrcpy(history[hist_count++], cmd);
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++)
            kstrcpy(history[i], history[i + 1]);
        kstrcpy(history[MAX_HISTORY - 1], cmd);
    }
    hist_idx = hist_count;
}

/* ── Readline with history navigation ────────────────────── */
static int readline(char *buf, int maxlen) {
    int pos = 0;
    kmemset(buf, 0, maxlen);
    char saved[MAX_CMD];
    kstrcpy(saved, buf);
    (void)saved;

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            vga_putchar('\n');
            buf[pos] = 0;
            return pos;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                buf[pos] = 0;
                vga_putchar('\b');
            }
        } else if (pos < maxlen - 1) {
            buf[pos++] = c;
            buf[pos]   = 0;
            vga_putchar(c);
        }
    }
}

/* ── Tokenizer ────────────────────────────────────────────── */
static int tokenize(char *line, char *argv[], int maxargs) {
    int argc = 0;
    char *p  = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        if (argc >= maxargs) break;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    return argc;
}

/* ── Helper: print uint with suffix ──────────────────────── */
static void print_bytes(size_t n) {
    char tmp[32];
    if (n >= 1024*1024) {
        kutoa((unsigned)(n / (1024*1024)), tmp, 10);
        kprintf("%s MB", tmp);
    } else if (n >= 1024) {
        kutoa((unsigned)(n / 1024), tmp, 10);
        kprintf("%s KB", tmp);
    } else {
        kutoa((unsigned)n, tmp, 10);
        kprintf("%s B", tmp);
    }
}

/* ── Port I/O (used by some commands) ────────────────────── */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ── CPU info via CPUID ───────────────────────────────────── */
static void do_cpuid(uint32_t leaf,
                     uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile ("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

/* ── Read TSC (crude timer) ───────────────────────────────── */
static uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* ════════════════════════════════════════════════════════════
   COMMANDS
   ════════════════════════════════════════════════════════════ */

static void cmd_help(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_print_colored("\n  KumOS Command Reference\n", VGA_YELLOW, VGA_BLACK);
    vga_print_colored("  ─────────────────────────────────────────\n", VGA_DARK_GREY, VGA_BLACK);

    const char *cmds[][2] = {
        {"help",     "show this help"},
        {"clear",    "clear the screen"},
        {"echo",     "print arguments"},
        {"cpuinfo",  "show CPU information"},
        {"meminfo",  "show memory usage"},
        {"uptime",   "show TSC-based uptime ticks"},
        {"ls",       "list files"},
        {"cat",      "print file contents"},
        {"write",    "write: write <file> <text>"},
        {"rm",       "delete a file"},
        {"hexdump",  "hexdump: hexdump <file>"},
        {"whoami",   "print current user"},
        {"uname",    "print OS info"},
        {"history",  "show command history"},
        {"calc",     "calc: calc <a> +|-|*|/ <b>"},
        {"color",    "color: color <fg 0-15> <bg 0-15>"},
        {"panic",    "trigger a kernel panic (test)"},
        {"reboot",   "reboot the machine"},
        {"halt",     "halt the machine"},
    };
    int n = sizeof(cmds) / sizeof(cmds[0]);
    for (int i = 0; i < n; i++) {
        vga_print_colored("  ", VGA_WHITE, VGA_BLACK);
        vga_print_colored(cmds[i][0], VGA_LIGHT_CYAN, VGA_BLACK);
        /* pad to 12 chars */
        int pad = 12 - (int)kstrlen(cmds[i][0]);
        for (int p = 0; p < pad; p++) vga_putchar(' ');
        vga_puts(cmds[i][1]);
        vga_putchar('\n');
    }
    vga_putchar('\n');
}

static void cmd_clear(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_clear();
    vga_print_banner();
}

static void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        vga_puts(argv[i]);
        if (i + 1 < argc) vga_putchar(' ');
    }
    vga_putchar('\n');
}

static void cmd_cpuinfo(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];

    do_cpuid(0, &eax, &ebx, &ecx, &edx);
    kmemcpy(vendor + 0, &ebx, 4);
    kmemcpy(vendor + 4, &edx, 4);
    kmemcpy(vendor + 8, &ecx, 4);
    vendor[12] = 0;

    vga_print_colored("\n  CPU Information\n", VGA_YELLOW, VGA_BLACK);
    kprintf("  Vendor    : %s\n", vendor);

    /* Brand string */
    char brand[49];
    kmemset(brand, 0, sizeof(brand));
    uint32_t b[4];
    do_cpuid(0x80000002, &b[0], &b[1], &b[2], &b[3]);
    kmemcpy(brand,      b, 16);
    do_cpuid(0x80000003, &b[0], &b[1], &b[2], &b[3]);
    kmemcpy(brand + 16, b, 16);
    do_cpuid(0x80000004, &b[0], &b[1], &b[2], &b[3]);
    kmemcpy(brand + 32, b, 16);
    kprintf("  Model     : %s\n", brand);

    do_cpuid(1, &eax, &ebx, &ecx, &edx);
    int stepping = eax & 0xF;
    int model    = (eax >> 4) & 0xF;
    int family   = (eax >> 8) & 0xF;
    kprintf("  Family    : %d  Model: %d  Stepping: %d\n", family, model, stepping);
    kprintf("  Features  : %s%s%s%s\n",
        (edx & (1<<25)) ? "SSE " : "",
        (edx & (1<<26)) ? "SSE2 " : "",
        (ecx & (1<<0))  ? "SSE3 " : "",
        (ecx & (1<<28)) ? "AVX " : "");
    vga_putchar('\n');
}

static void cmd_meminfo(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_print_colored("\n  Memory Information\n", VGA_YELLOW, VGA_BLACK);
    kprintf("  Total     : "); print_bytes(kmem_total()); vga_putchar('\n');
    kprintf("  Heap used : "); print_bytes(kmem_used());  vga_putchar('\n');
    kprintf("  Heap free : "); print_bytes(kmem_free_bytes()); vga_putchar('\n');
    vga_putchar('\n');
}

static void cmd_uptime(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint64_t tsc = rdtsc();
    char hi[32], lo[32];
    kutoa((unsigned)(tsc >> 32), hi, 10);
    kutoa((unsigned)(tsc & 0xFFFFFFFF), lo, 10);
    kprintf("TSC ticks: %s%s\n", hi, lo);
}

static void cmd_ls(int argc, char *argv[]) {
    (void)argc; (void)argv;
    if (fs_count == 0) {
        vga_puts("  (no files)\n");
        return;
    }
    vga_print_colored("\n  Name                          Size\n", VGA_YELLOW, VGA_BLACK);
    vga_print_colored("  ─────────────────────────────────\n", VGA_DARK_GREY, VGA_BLACK);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].used) continue;
        char szbuf[16];
        kitoa(fs[i].size, szbuf, 10);
        kprintf("  %-30s%s B\n", fs[i].name, szbuf);
    }
    vga_putchar('\n');
}

static void cmd_cat(int argc, char *argv[]) {
    if (argc < 2) { vga_puts("usage: cat <file>\n"); return; }
    file_t *f = fs_find(argv[1]);
    if (!f) { kprintf("cat: %s: no such file\n", argv[1]); return; }
    vga_puts(f->data);
    vga_putchar('\n');
}

static void cmd_write(int argc, char *argv[]) {
    if (argc < 3) { vga_puts("usage: write <file> <text>\n"); return; }
    file_t *f = fs_find(argv[1]);
    if (!f) {
        f = fs_create(argv[1]);
        if (!f) { vga_puts("write: filesystem full\n"); return; }
    }
    /* concatenate remaining args */
    kmemset(f->data, 0, MAX_FSIZE);
    for (int i = 2; i < argc; i++) {
        kstrcat(f->data, argv[i]);
        if (i + 1 < argc) kstrcat(f->data, " ");
    }
    f->size = (int)kstrlen(f->data);
    kprintf("wrote %d bytes to '%s'\n", f->size, f->name);
}

static void cmd_rm(int argc, char *argv[]) {
    if (argc < 2) { vga_puts("usage: rm <file>\n"); return; }
    file_t *f = fs_find(argv[1]);
    if (!f) { kprintf("rm: %s: no such file\n", argv[1]); return; }
    f->used = 0;
    fs_count--;
    kprintf("removed '%s'\n", argv[1]);
}

static void cmd_hexdump(int argc, char *argv[]) {
    if (argc < 2) { vga_puts("usage: hexdump <file>\n"); return; }
    file_t *f = fs_find(argv[1]);
    if (!f) { kprintf("hexdump: %s: no such file\n", argv[1]); return; }
    uint8_t *d = (uint8_t*)f->data;
    for (int i = 0; i < f->size; i += 16) {
        char addr[16]; kutoa((unsigned)i, addr, 16);
        kprintf("%08s  ", addr);
        for (int j = 0; j < 16; j++) {
            if (i + j < f->size) {
                char hb[4]; kutoa(d[i+j], hb, 16);
                if (d[i+j] < 16) kprintf("0%s ", hb);
                else              kprintf("%s ", hb);
            } else {
                kprintf("   ");
            }
            if (j == 7) kprintf(" ");
        }
        kprintf(" |");
        for (int j = 0; j < 16 && i + j < f->size; j++) {
            char c = (char)d[i+j];
            vga_putchar((c >= 32 && c < 127) ? c : '.');
        }
        kprintf("|\n");
    }
}

static void cmd_whoami(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_puts("root\n");
}

static void cmd_uname(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_puts("KumOS 1.0 i686 KumOS-Kernel\n");
}

static void cmd_history(int argc, char *argv[]) {
    (void)argc; (void)argv;
    for (int i = 0; i < hist_count; i++) {
        char idx[8]; kitoa(i + 1, idx, 10);
        kprintf("  %s\t%s\n", idx, history[i]);
    }
}

static void cmd_calc(int argc, char *argv[]) {
    if (argc != 4) { vga_puts("usage: calc <a> <+|-|*|/> <b>\n"); return; }
    int a = katoi(argv[1]);
    int b = katoi(argv[3]);
    char op = argv[2][0];
    int result = 0;
    if      (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/') {
        if (b == 0) { vga_puts("calc: division by zero\n"); return; }
        result = a / b;
    } else {
        vga_puts("calc: unknown operator\n"); return;
    }
    char res[32]; kitoa(result, res, 10);
    kprintf("%s\n", res);
}

static void cmd_color(int argc, char *argv[]) {
    if (argc != 3) { vga_puts("usage: color <fg 0-15> <bg 0-15>\n"); return; }
    int fg = katoi(argv[1]);
    int bg = katoi(argv[2]);
    if (fg < 0 || fg > 15 || bg < 0 || bg > 15) {
        vga_puts("color: values must be 0-15\n"); return;
    }
    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("Color changed. Type 'clear' to apply to full screen.\n");
}

static void cmd_panic(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_print_colored("\n  *** KERNEL PANIC ***\n", VGA_WHITE, VGA_RED);
    vga_print_colored("  This was intentional. System halted.\n", VGA_YELLOW, VGA_RED);
    __asm__ volatile ("cli; hlt");
}

static void cmd_reboot(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_puts("Rebooting...\n");
    /* Pulse keyboard controller reset line */
    outb(0x64, 0xFE);
    __asm__ volatile ("cli; hlt");
}

static void cmd_halt(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_puts("System halted. Goodbye.\n");
    __asm__ volatile ("cli; hlt");
}

/* ── Command dispatch ─────────────────────────────────────── */
typedef struct {
    const char *name;
    void (*fn)(int, char**);
} command_t;

static const command_t commands[] = {
    {"help",    cmd_help},
    {"clear",   cmd_clear},
    {"echo",    cmd_echo},
    {"cpuinfo", cmd_cpuinfo},
    {"meminfo", cmd_meminfo},
    {"uptime",  cmd_uptime},
    {"ls",      cmd_ls},
    {"cat",     cmd_cat},
    {"write",   cmd_write},
    {"rm",      cmd_rm},
    {"hexdump", cmd_hexdump},
    {"whoami",  cmd_whoami},
    {"uname",   cmd_uname},
    {"history", cmd_history},
    {"calc",    cmd_calc},
    {"color",   cmd_color},
    {"panic",   cmd_panic},
    {"reboot",  cmd_reboot},
    {"halt",    cmd_halt},
    {NULL, NULL}
};

static void dispatch(char *line) {
    if (!line || !*line) return;

    char argv_buf[MAX_CMD];
    kstrncpy(argv_buf, line, MAX_CMD - 1);

    char *argv[MAX_ARGS];
    int   argc = tokenize(argv_buf, argv, MAX_ARGS);
    if (!argc) return;

    hist_add(line);

    for (int i = 0; commands[i].name; i++) {
        if (kstrcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }
    kprintf("kum: command not found: %s\n", argv[0]);
}

/* ── Boot greeting ────────────────────────────────────────── */
static void print_greeting(void) {
    vga_print_colored(
        "  Welcome to KumOS v1.0\n"
        "  A minimal OS written in C\n",
        VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print_colored(
        "  Type 'help' for available commands.\n\n",
        VGA_DARK_GREY, VGA_BLACK);
}

/* ── Main shell loop ──────────────────────────────────────── */
void shell_run(void) {
    print_greeting();

    char line[MAX_CMD];
    while (1) {
        vga_print_colored("root@kumos", VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print_colored(":/ $ ", VGA_LIGHT_CYAN, VGA_BLACK);
        readline(line, MAX_CMD);
        dispatch(line);
    }
}
