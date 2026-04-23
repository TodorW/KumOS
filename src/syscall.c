
#include "syscall.h"
#include "idt.h"
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "sched.h"
#include "fs.h"
#include "fat12.h"
#include "paging.h"
#include "rtc.h"
#include "serial.h"
#include "kstring.h"
#include "vfs.h"
#include "pipe.h"
#include "elf.h"
#include "signal.h"
#include "kstring.h"
#include <stdint.h>

extern void isr128(void);

static uint32_t sc_exit(uint32_t code, uint32_t b, uint32_t c) {
    (void)b; (void)c;

    sched_exit();
    (void)code;
    return 0;
}

static uint32_t sc_write(uint32_t buf_addr, uint32_t len, uint32_t c) {
    (void)c;

    if (buf_addr == 0 || buf_addr > 0x20000000) return (uint32_t)-1;
    if (len > 4096) len = 4096;
    const char *buf = (const char *)buf_addr;
    for (uint32_t i = 0; i < len; i++) vga_putchar(buf[i]);
    return len;
}

static uint32_t sc_read(uint32_t buf_addr, uint32_t len, uint32_t c) {
    (void)c;
    if (buf_addr == 0 || len == 0) return 0;
    if (len > 256) len = 256;
    char *buf = (char *)buf_addr;
    return (uint32_t)keyboard_getline(buf, (int)len);
}

static uint32_t sc_getpid(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    return (uint32_t)sched_current()->pid;
}

static uint32_t sc_sleep(uint32_t ms, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    timer_sleep(ms);
    return 0;
}

static uint32_t sc_yield(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    sched_yield();
    return 0;
}

static uint32_t sc_sbrk(uint32_t inc, uint32_t b, uint32_t c) {
    (void)b; (void)c;

    if (inc == 0) return heap_brk();
    void *p = vmalloc(inc);
    return p ? (uint32_t)p : (uint32_t)-1;
}

static uint32_t sc_uptime(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    return timer_seconds();
}

static uint32_t sc_getkey(uint32_t a, uint32_t b, uint32_t c) {
    (void)a; (void)b; (void)c;
    return (uint32_t)keyboard_getchar_blocking();
}

static uint32_t sc_putchar(uint32_t ch, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    vga_putchar((char)ch);
    return 0;
}

#define MAX_USER_FD  8
static struct { char name[32]; int open; uint32_t pos; } ufd[MAX_USER_FD];

static uint32_t sc_open(uint32_t name_addr, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    if (!name_addr) return (uint32_t)-1;
    const char *name = (const char *)name_addr;

    for (int i = 0; i < MAX_USER_FD; i++) {
        if (!ufd[i].open) {

            fs_file_t *mf = fs_find(name);
            if (!mf && !fat12_mounted()) return (uint32_t)-1;
            kstrcpy(ufd[i].name, name);
            ufd[i].open = 1;
            ufd[i].pos  = 0;
            return (uint32_t)i;
        }
    }
    return (uint32_t)-1;
}

static uint32_t sc_close(uint32_t fd, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    if (fd >= MAX_USER_FD) return (uint32_t)-1;
    ufd[fd].open = 0;
    return 0;
}

static uint32_t sc_fread(uint32_t fd, uint32_t buf_addr, uint32_t sz) {
    if (fd >= MAX_USER_FD || !ufd[fd].open) return (uint32_t)-1;
    if (!buf_addr || sz == 0) return 0;
    char *buf = (char *)buf_addr;

    fs_file_t *mf = fs_find(ufd[fd].name);
    if (mf) {
        uint32_t avail = mf->size - ufd[fd].pos;
        if (sz > avail) sz = avail;
        kmemcpy(buf, mf->data + ufd[fd].pos, sz);
        ufd[fd].pos += sz;
        return sz;
    }

    if (fat12_mounted()) {
        static uint8_t tbuf[4096];
        int n = fat12_read(ufd[fd].name, tbuf, sizeof(tbuf));
        if (n < 0) return (uint32_t)-1;
        uint32_t avail = (uint32_t)n - ufd[fd].pos;
        if (sz > avail) sz = avail;
        kmemcpy(buf, tbuf + ufd[fd].pos, sz);
        ufd[fd].pos += sz;
        return sz;
    }
    return (uint32_t)-1;
}

static uint32_t sc_fwrite(uint32_t fd, uint32_t buf_addr, uint32_t sz) {
    if (fd >= MAX_USER_FD || !ufd[fd].open) return (uint32_t)-1;
    if (!buf_addr || sz == 0) return 0;
    const char *buf = (const char *)buf_addr;

    fs_file_t *mf = fs_find(ufd[fd].name);
    if (mf) {
        fs_write(ufd[fd].name, buf);
        return sz;
    }

    if (fat12_mounted()) {
        return fat12_write(ufd[fd].name, buf, sz) == 0 ? sz : (uint32_t)-1;
    }
    return (uint32_t)-1;
}

static uint32_t sc_listdir(uint32_t buf_addr, uint32_t sz, uint32_t c) {
    (void)c;
    if (!buf_addr || sz < 64) return 0;
    char *buf = (char *)buf_addr;
    uint32_t pos = 0;

    fat12_entry_t entries[32];
    int n = fs_file_count();
    (void)n;

    if (fat12_mounted()) {
        int dc = fat12_list(entries, 32);
        for (int i = 0; i < dc && pos + 16 < sz; i++) {
            kstrcpy(buf + pos, entries[i].name);
            pos += kstrlen(entries[i].name);
            buf[pos++] = '\n';
        }
    }
    buf[pos] = 0;
    return pos;
}

static uint32_t sc_waitpid(uint32_t pid, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return (uint32_t)sched_waitpid((int)pid);
}

static uint32_t sc_wait(uint32_t code_ptr, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return (uint32_t)sched_wait((int *)(code_ptr ? code_ptr : 0));
}

static uint32_t sc_gettime(uint32_t buf_addr, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    if (!buf_addr) return (uint32_t)-1;
    rtc_time_t t = rtc_read();
    kmemcpy((void *)buf_addr, &t, sizeof(rtc_time_t));
    return 0;
}

static uint32_t sc_serial_write(uint32_t buf, uint32_t len, uint32_t c) {
    (void)c;
    if (!buf || len > 4096) return (uint32_t)-1;
    if (serial_ready()) serial_write(COM1, (const char *)buf, len);
    return len;
}

static uint32_t sc_vfs_open(uint32_t path, uint32_t flags, uint32_t c) {
    (void)c;
    if (!path) return (uint32_t)-1;
    return (uint32_t)vfs_open((const char *)path, (int)flags);
}
static uint32_t sc_vfs_close(uint32_t fd, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    return (uint32_t)vfs_close((int)fd);
}
static uint32_t sc_vfs_read(uint32_t fd, uint32_t buf, uint32_t len) {
    if (!buf) return (uint32_t)-1;
    return (uint32_t)vfs_read((int)fd, (void *)buf, len);
}
static uint32_t sc_vfs_write(uint32_t fd, uint32_t buf, uint32_t len) {
    if (!buf) return (uint32_t)-1;
    return (uint32_t)vfs_write((int)fd, (const void *)buf, len);
}
static uint32_t sc_vfs_readdir(uint32_t path, uint32_t buf, uint32_t sz) {
    if (!path||!buf) return 0;
    return (uint32_t)vfs_readdir((const char *)path, (char *)buf, sz);
}
static uint32_t sc_vfs_stat(uint32_t path, uint32_t st, uint32_t c) {
    (void)c;
    if (!path||!st) return (uint32_t)-1;
    return (uint32_t)vfs_stat((const char *)path, (vfs_stat_t *)st);
}
static uint32_t sc_vfs_unlink(uint32_t path, uint32_t b, uint32_t c) {
    (void)b;(void)c;
    if (!path) return (uint32_t)-1;
    return (uint32_t)vfs_unlink((const char *)path);
}
static uint32_t sc_vfs_pipe(uint32_t fds, uint32_t b, uint32_t c) {
    (void)b;(void)c;
    if (!fds) return (uint32_t)-1;
    return (uint32_t)vfs_pipe((int *)fds);
}
static uint32_t sc_vfs_dup2(uint32_t old, uint32_t nw, uint32_t c) {
    (void)c;
    return (uint32_t)vfs_dup2((int)old, (int)nw);
}
static uint32_t sc_vfs_getcwd(uint32_t buf, uint32_t sz, uint32_t c) {
    (void)c;
    if (!buf) return (uint32_t)-1;
    return (uint32_t)vfs_getcwd((char *)buf, sz);
}
static uint32_t sc_vfs_chdir(uint32_t path, uint32_t b, uint32_t c) {
    (void)b;(void)c;
    if (!path) return (uint32_t)-1;
    return (uint32_t)vfs_chdir((const char *)path);
}
static uint32_t sc_isatty(uint32_t fd, uint32_t b, uint32_t c) {
    (void)b;(void)c;
    return (uint32_t)vfs_isatty((int)fd);
}

static uint32_t sc_kill(uint32_t pid, uint32_t sig, uint32_t c) {
    (void)c;
    return (uint32_t)signal_send((int)pid, (int)sig);
}

static uint32_t sc_signal_set(uint32_t sig, uint32_t handler, uint32_t c) {
    (void)c;
    return (uint32_t)signal_set_handler((int)sig, (sighandler_t)handler);
}
static uint32_t sc_getppid(uint32_t a, uint32_t b, uint32_t c) {
    (void)a;(void)b;(void)c;
    task_t *t = sched_current();
    return (uint32_t)t->parent_pid;
}

static uint32_t sc_execve(uint32_t path_addr, uint32_t argv_addr, uint32_t envp_addr) {
    (void)envp_addr;
    if (!path_addr) return (uint32_t)-1;
    const char *path = (const char *)path_addr;

    char upper[64]; int i=0;
    while(path[i]&&i<63){char ch=path[i];if(ch>='a'&&ch<='z')ch-=32;upper[i++]=ch;}
    upper[i]=0;
    if(!kstrchr(upper,'.')) { kstrcat(upper,".ELF"); }

    elf_load_result_t r = elf_load_disk(upper);
    if (r.error != 0) return (uint32_t)-1;

    task_t *cur = sched_current();

    if (cur->page_dir_phys) {
        paging_free_user(cur->page_dir_phys);
        cur->page_dir_phys = 0;
    }

    uint32_t new_dir = paging_clone_dir();
    if (!new_dir) return (uint32_t)-1;
    cur->page_dir_phys = new_dir;
    paging_switch(new_dir);

    elf_load_result_t r2 = elf_load_disk(upper);
    if (r2.error != 0) { paging_switch(0); return (uint32_t)-1; }

    kstrcpy(cur->name, upper);
    cur->argc = 0;
    if (argv_addr) {
        char **argv = (char **)argv_addr;
        while (argv[cur->argc] && cur->argc < 15) {
            cur->argv[cur->argc] = argv[cur->argc];
            cur->argc++;
        }
    }
    cur->argv[cur->argc] = 0;

    uint32_t user_stack_top = 0x40000000;
    uint32_t user_esp       = user_stack_top - 4;

    for (uint32_t va = user_stack_top - 16384; va < user_stack_top; va += PAGE_SIZE) {
        if (!paging_is_mapped(va)) {
            uint32_t phys = pmm_alloc();
            if (phys) { kmemset((void*)phys,0,PAGE_SIZE); paging_map(va,phys,PAGE_WRITE|PAGE_USER); }
        }
    }

    uint32_t *sp = (uint32_t *)(uintptr_t)user_esp;
    uint32_t *ksp = (uint32_t *)((uint8_t *)cur->stack + cur->stack_size);

    *--ksp = 0x23;
    *--ksp = user_esp;
    *--ksp = 0x202;
    *--ksp = 0x1B;
    *--ksp = r2.entry;
    for(int j=0;j<8;j++) *--ksp = 0;
    *--ksp = 0x23;
    *--ksp = 0; *--ksp = 0;

    cur->esp = (uint32_t)(uintptr_t)ksp;
    (void)sp;
    return 0;
}

static uint32_t sc_exec(uint32_t path_addr, uint32_t b, uint32_t c) {
    return sc_execve(path_addr, b, c);
}

static uint32_t sc_fork(uint32_t a, uint32_t b, uint32_t c) {
    (void)a;(void)b;(void)c;
    task_t *parent = sched_current();

    uint32_t child_dir = paging_clone_dir();
    if (!child_dir) return (uint32_t)-1;

    uint32_t *child_kstack = kmalloc(SCHED_STACK_SIZE);
    if (!child_kstack) { paging_free_user(child_dir); return (uint32_t)-1; }

    int child_pid = sched_spawn(parent->name, 0, parent->kum_level);
    if (child_pid < 0) {
        kfree(child_kstack);
        paging_free_user(child_dir);
        return (uint32_t)-1;
    }

    task_t *child = sched_get_task(child_pid);
    if (!child) return (uint32_t)-1;

    child->parent_pid    = parent->pid;
    child->page_dir_phys = child_dir;
    kstrcpy(child->cwd,   parent->cwd);
    child->argc          = parent->argc;
    for(int j=0;j<parent->argc&&j<15;j++) child->argv[j]=parent->argv[j];

    if (child->stack) kfree(child->stack);
    child->stack      = child_kstack;
    child->stack_size = SCHED_STACK_SIZE;

    uint32_t *sp = (uint32_t *)((uint8_t *)child_kstack + SCHED_STACK_SIZE);
    *--sp = 0x23;
    *--sp = parent->esp;
    *--sp = 0x202;
    *--sp = 0x1B;
    *--sp = 0;
    for(int j=0;j<8;j++) *--sp=0;
    *--sp = 0x23;
    *--sp = 0; *--sp = 0;
    child->esp = (uint32_t)(uintptr_t)sp;

    return (uint32_t)child_pid;
}

static uint32_t sc_tcp_connect(uint32_t ip, uint32_t port, uint32_t x) {
    (void)x; return (uint32_t)tcp_connect(ip,(uint16_t)port);
}
static uint32_t sc_tcp_send(uint32_t s, uint32_t buf, uint32_t len) {
    return (uint32_t)tcp_send((int)s,(const void*)buf,(uint16_t)len);
}
static uint32_t sc_tcp_recv(uint32_t s, uint32_t buf, uint32_t len) {
    return (uint32_t)tcp_recv((int)s,(void*)buf,(uint16_t)len);
}
static uint32_t sc_tcp_close(uint32_t s, uint32_t b, uint32_t x) {
    (void)b;(void)x; tcp_close((int)s); return 0;
}

static uint32_t sc_select(uint32_t nfds, uint32_t rfds_addr, uint32_t wfds_addr) {
    uint32_t timeout_ms = 5000;
    uint32_t deadline = timer_ticks() + timeout_ms / 10;
    int ready = 0;

    while (timer_ticks() < deadline) {
        for (uint32_t fd = 0; fd < nfds && fd < 32; fd++) {
            if (rfds_addr && (*(uint32_t*)rfds_addr & (1u<<fd))) {
                vfs_fd_t *f = vfs_get_fd((int)fd);
                if (!f) continue;
                if (f->type == VFS_PIPE && pipe_has_data(f->pipe_id)) {
                    ready++; continue;
                }
                if (f->type == VFS_DEV && f->fd_data == 0) {
                    if (keyboard_has_input()) ready++;
                }
            }
        }
        if (ready) break;
        sched_yield();
    }
    return (uint32_t)ready;
}

static uint32_t sc_poll(uint32_t fds_addr, uint32_t nfds, uint32_t timeout_ms) {
    return sc_select(nfds, fds_addr, 0);
    (void)timeout_ms;
}
typedef uint32_t (*syscall_fn)(uint32_t, uint32_t, uint32_t);

static syscall_fn syscall_table[SYSCALL_MAX] = {
    [SYS_EXIT]    = sc_exit,
    [SYS_WRITE]   = sc_vfs_write,
    [SYS_READ]    = sc_vfs_read,
    [SYS_GETPID]  = sc_getpid,
    [SYS_SLEEP]   = sc_sleep,
    [SYS_YIELD]   = sc_yield,
    [SYS_SBRK]    = sc_sbrk,
    [SYS_UPTIME]  = sc_uptime,
    [SYS_GETKEY]  = sc_getkey,
    [SYS_PUTCHAR] = sc_putchar,
    [SYS_OPEN]    = sc_vfs_open,
    [SYS_CLOSE]   = sc_vfs_close,
    [SYS_FREAD]   = sc_vfs_read,
    [SYS_FWRITE]  = sc_vfs_write,
    [SYS_LISTDIR] = sc_vfs_readdir,
    [SYS_WAITPID] = sc_waitpid,
    [SYS_WAIT]    = sc_wait,
    [SYS_GETTIME] = sc_gettime,
    [SYS_SERIAL]  = sc_serial_write,
    [SYS_EXEC]    = sc_exec,
    [SYS_FORK]    = sc_fork,
    [SYS_PIPE]    = sc_vfs_pipe,
    [SYS_DUP2]    = sc_vfs_dup2,
    [SYS_GETCWD]  = sc_vfs_getcwd,
    [SYS_CHDIR]   = sc_vfs_chdir,
    [SYS_STAT]    = sc_vfs_stat,
    [SYS_UNLINK]  = sc_vfs_unlink,
    [SYS_GETPPID] = sc_getppid,
    [SYS_ISATTY]  = sc_isatty,
    [SYS_KILL]    = sc_kill,
    [SYS_SIGNAL]  = sc_signal_set,
    [SYS_TCP_CONNECT] = sc_tcp_connect,
    [SYS_TCP_SEND]    = sc_tcp_send,
    [SYS_TCP_RECV]    = sc_tcp_recv,
    [SYS_TCP_CLOSE]   = sc_tcp_close,
    [SYS_EXECVE]      = sc_execve,
    [SYS_SELECT]      = sc_select,
    [SYS_POLL]        = sc_poll,
};

uint32_t syscall_dispatch(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    if (num >= SYSCALL_MAX || !syscall_table[num]) return (uint32_t)-1;
    return syscall_table[num](a, b, c);
}

void syscall_init(void) {
    kmemset(ufd, 0, sizeof(ufd));

    idt_set_raw(0x80, (uint32_t)isr128, 0x08, 0xEE);
}