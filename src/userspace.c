
#include "userspace.h"
#include "syscall.h"
#include "sched.h"
#include "paging.h"
#include "gdt.h"
#include "vga.h"
#include "kstring.h"
#include "kmalloc.h"
#include "timer.h"
#include <stdint.h>

#define USER_STACK_SIZE  8192

#define USER_CS  0x1B
#define USER_DS  0x23

static void user_task_setup(task_t *t, void (*entry)(void), uint32_t user_esp) {

    uint32_t *sp = (uint32_t *)((uint8_t *)t->stack + t->stack_size);

    *--sp = USER_DS;
    *--sp = user_esp;
    *--sp = 0x202;
    *--sp = USER_CS;
    *--sp = (uint32_t)entry;

    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;

    *--sp = USER_DS;

    *--sp = 0;
    *--sp = 0;

    t->esp = (uint32_t)sp;
}

int user_spawn(const char *name, void (*entry)(void)) {

    uint32_t *kstack = kmalloc(SCHED_STACK_SIZE);
    if (!kstack) return -1;

    void *ustack = vmalloc(USER_STACK_SIZE);
    if (!ustack) { kfree(kstack); return -1; }

    for (uint32_t off = 0; off < USER_STACK_SIZE; off += PAGE_SIZE) {
        uint32_t virt = (uint32_t)ustack + off;
        uint32_t phys = paging_virt_to_phys(virt);
        if (phys) paging_map(virt, phys, PAGE_WRITE | PAGE_USER);
    }

    int pid = sched_spawn(name, entry, 0);
    if (pid < 0) { kfree(kstack); vmfree(ustack); return -1; }

    task_t *t = 0;

    t = sched_get_task(pid);
    if (!t) return pid;

    uint32_t user_esp = (uint32_t)ustack + USER_STACK_SIZE - 4;
    user_task_setup(t, entry, user_esp);

    return pid;
}

void uprog_hello(void) {
    sys_puts("\n  *** Hello from ring 3! ***\n\n");
    sys_puts("  This program runs in userspace (CPL=3).\n");
    sys_puts("  It can only talk to the kernel via INT 0x80.\n\n");

    sys_puts("  PID:    ");
    int pid = sys_getpid();

    char pbuf[12]; int i = 10;
    pbuf[11] = 0;
    if (pid == 0) { pbuf[i--] = '0'; }
    else { int p = pid; while(p){ pbuf[i--]='0'+(p%10); p/=10; } }
    sys_puts(pbuf + i + 1);
    sys_puts("\n");

    sys_puts("  Uptime: ");
    uint32_t up = sys_uptime();
    i = 10; pbuf[11] = 0;
    if (up == 0) { pbuf[i--] = '0'; }
    else { uint32_t u = up; while(u){ pbuf[i--]='0'+(u%10); u/=10; } }
    sys_puts(pbuf + i + 1);
    sys_puts("s\n\n");

    sys_puts("  Press any key to exit...\n");
    sys_getkey();
    sys_exit(0);
}

void uprog_counter(void) {
    sys_puts("\n  Counter (ring 3):\n  ");
    for (int i = 0; i <= 9; i++) {
        sys_putchar('0' + i);
        sys_putchar(' ');
        sys_sleep(200);
    }
    sys_puts("\n  Done!\n\n");
    sys_exit(0);
}

void uprog_top(void) {
    sys_puts("\n  [top] Refreshing 5 times (1s interval)...\n");
    for (int pass = 0; pass < 5; pass++) {
        sys_puts("  --- pass ");
        sys_putchar('1' + pass);
        sys_puts(" ---  uptime: ");
        uint32_t u = sys_uptime();
        char buf[12]; int i = 10; buf[11]=0;
        if (!u) { buf[i--]='0'; }
        else { uint32_t x=u; while(x){buf[i--]='0'+(x%10);x/=10;} }
        sys_puts(buf+i+1);
        sys_puts("s\n");
        sys_sleep(1000);
        sys_yield();
    }
    sys_puts("  [top] Done.\n\n");
    sys_exit(0);
}

void uprog_echo(void) {
    sys_puts("\n  [echo] Type something (ring-3 read via syscall):\n  > ");
    char buf[128];
    int n = sys_read(buf, 127);
    buf[n] = 0;
    sys_puts("  You typed: '");
    sys_puts(buf);
    sys_puts("'\n\n");
    sys_exit(0);
}

void uprog_cat(void) {
    sys_puts("\n  [cat] Reading README.TXT from disk via syscall...\n\n");
    int fd = sys_open("README.TXT");
    if ((uint32_t)fd == (uint32_t)-1) {

        fd = sys_open("readme.txt");
    }
    if ((uint32_t)fd == (uint32_t)-1) {
        sys_puts("  File not found.\n\n");
        sys_exit(1);
    }
    char buf[512];
    int n = sys_fread(fd, buf, 511);
    if (n > 0) {
        buf[n] = 0;
        sys_puts(buf);
        sys_puts("\n");
    }
    sys_close(fd);
    sys_exit(0);
}

uprog_t uprog_list[] = {
    { "hello",   uprog_hello   },
    { "counter", uprog_counter },
    { "top",     uprog_top     },
    { "echo",    uprog_echo    },
    { "cat",     uprog_cat     },
};
int uprog_count = 5;