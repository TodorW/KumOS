
#include "vfs.h"
#include "pipe.h"
#include "fs.h"
#include "fat12.h"
#include "keyboard.h"
#include "vga.h"
#include "kstring.h"
#include "sched.h"
#include "gui.h"
#include <stdint.h>

static vfs_fd_t fd_table[VFS_MAX_FD];
static char     cwd[VFS_MAX_PATH] = "/disk";

static vfs_mount_t mounts[VFS_MAX_MOUNTS];
static int         num_mounts = 0;

static const char *strip_prefix(const char *path, const char *prefix) {
    int plen = (int)kstrlen(prefix);
    if (kstrncmp(path, prefix, (uint32_t)plen) == 0) {
        const char *rest = path + plen;
        if (*rest == '/') rest++;
        return rest;
    }
    return 0;
}

static int find_mount(const char *path, const char **local) {

    int best = -1, best_len = 0;
    for (int i = 0; i < num_mounts; i++) {
        if (!mounts[i].active) continue;
        int plen = (int)kstrlen(mounts[i].prefix);
        if (kstrncmp(path, mounts[i].prefix, (uint32_t)plen) == 0) {
            if (plen > best_len) { best = i; best_len = plen; }
        }
    }
    if (best >= 0 && local) {
        *local = path + best_len;
        if (**local == '/') (*local)++;
    }
    return best;
}

#define MEM_MAX_OPEN 16
static struct { fs_file_t *f; uint32_t pos; int used; } mem_open[MEM_MAX_OPEN];

static int mem_vfs_open(const char *path, int flags) {
    fs_file_t *f = fs_find(path);
    if (!f) {
        if (flags & O_CREAT) {
            if (fs_create(path, "", 0) < 0) return -1;
            f = fs_find(path);
        }
        if (!f) return -1;
    }
    if (flags & O_TRUNC) fs_write(path, "");
    for (int i = 0; i < MEM_MAX_OPEN; i++) {
        if (!mem_open[i].used) {
            mem_open[i].f   = f;
            mem_open[i].pos = (flags & O_APPEND) ? f->size : 0;
            mem_open[i].used = 1;
            return i;
        }
    }
    return -1;
}
static int mem_vfs_close(int d) {
    if (d >= 0 && d < MEM_MAX_OPEN) mem_open[d].used = 0;
    return 0;
}
static int mem_vfs_read(int d, void *buf, uint32_t len) {
    if (d < 0 || d >= MEM_MAX_OPEN || !mem_open[d].used) return -1;
    fs_file_t *f = mem_open[d].f;
    uint32_t avail = f->size - mem_open[d].pos;
    if (len > avail) len = avail;
    kmemcpy(buf, f->data + mem_open[d].pos, len);
    mem_open[d].pos += len;
    return (int)len;
}
static int mem_vfs_write(int d, const void *buf, uint32_t len) {
    if (d < 0 || d >= MEM_MAX_OPEN || !mem_open[d].used) return -1;

    char tmp[4096];
    fs_file_t *f = mem_open[d].f;
    uint32_t keep = mem_open[d].pos < f->size ? mem_open[d].pos : f->size;
    kmemcpy(tmp, f->data, keep);
    if (keep + len < sizeof(tmp) - 1) {
        kmemcpy(tmp + keep, buf, len);
        tmp[keep+len] = 0;
        fs_write(f->name, tmp);
        mem_open[d].pos += len;
    }
    return (int)len;
}
static int mem_vfs_stat(const char *path, vfs_stat_t *st) {
    fs_file_t *f = fs_find(path);
    if (!f) return -1;
    st->size = f->size; st->type = VFS_FILE;
    kstrcpy(st->name, f->name);
    return 0;
}
static int mem_vfs_readdir(const char *path, char *buf, uint32_t sz) {
    (void)path;

    uint32_t pos = 0;

    fat12_entry_t entries[32];
    (void)entries;

    buf[0] = 0; (void)pos; (void)sz;
    return 0;
}
static int mem_vfs_unlink(const char *path) { return fs_delete(path); }

static vfs_ops_t mem_ops = {
    mem_vfs_open, mem_vfs_close, mem_vfs_read, mem_vfs_write,
    mem_vfs_stat, mem_vfs_readdir, mem_vfs_unlink, 0
};

#define DISK_BUF_SIZE  4096
#define DISK_MAX_OPEN  8
static struct {
    char     name[64];
    uint8_t  buf[DISK_BUF_SIZE];
    uint32_t size, pos;
    int      writable, used;
} disk_open[DISK_MAX_OPEN];

static int disk_vfs_open(const char *path, int flags) {
    if (!fat12_mounted()) return -1;

    char upper[64]; int i=0;
    while (path[i] && i<63) {
        char c=path[i];
        if(c>='a'&&c<='z') c-=32;
        upper[i++]=c;
    }
    upper[i]=0;
    for (int s = 0; s < DISK_MAX_OPEN; s++) {
        if (!disk_open[s].used) {
            int n = fat12_read(upper, disk_open[s].buf, DISK_BUF_SIZE);
            if (n < 0) {
                if (flags & O_CREAT) {

                    fat12_write(upper, "", 0);
                    n = 0;
                } else return -1;
            }
            disk_open[s].size = (uint32_t)n;
            disk_open[s].pos  = (flags & O_APPEND) ? (uint32_t)n : 0;
            if (flags & O_TRUNC) disk_open[s].size = disk_open[s].pos = 0;
            disk_open[s].writable = (flags & (O_WRONLY|O_RDWR|O_CREAT)) ? 1 : 0;
            disk_open[s].used = 1;
            kstrcpy(disk_open[s].name, upper);
            return s;
        }
    }
    return -1;
}
static int disk_vfs_close(int d) {
    if (d < 0 || d >= DISK_MAX_OPEN || !disk_open[d].used) return -1;
    if (disk_open[d].writable && disk_open[d].size > 0)
        fat12_write(disk_open[d].name, disk_open[d].buf, disk_open[d].size);
    disk_open[d].used = 0;
    return 0;
}
static int disk_vfs_read(int d, void *buf, uint32_t len) {
    if (d < 0 || d >= DISK_MAX_OPEN || !disk_open[d].used) return -1;
    uint32_t avail = disk_open[d].size - disk_open[d].pos;
    if (len > avail) len = avail;
    kmemcpy(buf, disk_open[d].buf + disk_open[d].pos, len);
    disk_open[d].pos += len;
    return (int)len;
}
static int disk_vfs_write(int d, const void *buf, uint32_t len) {
    if (d < 0 || d >= DISK_MAX_OPEN || !disk_open[d].used) return -1;
    if (disk_open[d].pos + len >= DISK_BUF_SIZE) len = DISK_BUF_SIZE - disk_open[d].pos - 1;
    kmemcpy(disk_open[d].buf + disk_open[d].pos, buf, len);
    disk_open[d].pos += len;
    if (disk_open[d].pos > disk_open[d].size) disk_open[d].size = disk_open[d].pos;
    return (int)len;
}
static int disk_vfs_stat(const char *path, vfs_stat_t *st) {
    if (!fat12_mounted()) return -1;
    char upper[64]; int i=0;
    while(path[i]&&i<63){char c=path[i];if(c>='a'&&c<='z')c-=32;upper[i++]=c;}upper[i]=0;
    fat12_entry_t entries[64];
    int n = fat12_list(entries, 64);
    for (int j=0;j<n;j++) {
        if (kstrcmp(entries[j].name, upper)==0) {
            st->size = entries[j].size;
            st->type = VFS_FILE;
            kstrcpy(st->name, entries[j].name);
            return 0;
        }
    }
    return -1;
}
static int disk_vfs_readdir(const char *path, char *buf, uint32_t sz) {
    (void)path;
    if (!fat12_mounted()) return 0;
    fat12_entry_t entries[64];
    int n = fat12_list(entries, 64);
    uint32_t pos = 0;
    for (int i=0;i<n&&pos+20<sz;i++) {
        kstrcpy(buf+pos, entries[i].name);
        pos += kstrlen(entries[i].name);
        buf[pos++] = '\n';
    }
    buf[pos] = 0;
    return (int)pos;
}
static int disk_vfs_unlink(const char *path) {
    char upper[64]; int i=0;
    while(path[i]&&i<63){char c=path[i];if(c>='a'&&c<='z')c-=32;upper[i++]=c;}upper[i]=0;
    return fat12_delete(upper);
}

static vfs_ops_t disk_ops = {
    disk_vfs_open, disk_vfs_close, disk_vfs_read, disk_vfs_write,
    disk_vfs_stat, disk_vfs_readdir, disk_vfs_unlink, 0
};

static int dev_vfs_open(const char *path, int flags) {
    (void)flags;
    if (kstrcmp(path,"stdin")==0||kstrcmp(path,"tty")==0) return 0;
    if (kstrcmp(path,"stdout")==0)                         return 1;
    if (kstrcmp(path,"stderr")==0)                         return 2;
    if (kstrcmp(path,"null")==0)                           return 3;
    return -1;
}
static int dev_vfs_close(int d) { (void)d; return 0; }
static int dev_vfs_read(int d, void *buf, uint32_t len) {
    if (d == 0) return keyboard_getline((char*)buf, (int)len);
    if (d == 3) return 0;
    return -1;
}
static vfs_stdout_sink_t stdout_sink = 0;

void vfs_set_stdout_sink(vfs_stdout_sink_t sink) { stdout_sink = sink; }

static int dev_vfs_write(int d, const void *buf, uint32_t len) {
    if (d == 1 || d == 2) {
        const char *s = (const char *)buf;
        /* A process with no stdout_sink wired (anything not launched via
           the GUI's own Run button - e.g. a background daemon started
           from the pre-GUI kush shell that goes on printing after the GUI
           takes over) used to fall through to vga_putchar() unconditionally.
           That's fine in real text mode, but gui.c's VBE/LFB mode reuses
           the exact same physical VRAM as the legacy 0xB8000 window (see
           vga_font_snapshot()'s comment), so those writes land inside the
           live, currently-displayed framebuffer instead of a buffer nobody
           is showing - confirmed live as the source of a long-open "GUI
           corrupts near the border" report (`crond.elf &` then `exit` to
           the GUI reliably painted RGB static over the desktop/icon column
           within seconds). With no sink and the GUI up, there's nowhere
           legitimate for this output to go - drop it instead of scribbling
           into video memory. */
        if (stdout_sink)          { for (uint32_t i = 0; i < len; i++) stdout_sink(s[i]); }
        else if (!gui_is_active()) { for (uint32_t i = 0; i < len; i++) vga_putchar(s[i]); }
        return (int)len;
    }
    if (d == 3) return (int)len;
    return -1;
}
static int dev_vfs_stat(const char *path, vfs_stat_t *st) {
    st->size = 0; st->type = VFS_DEV;
    kstrcpy(st->name, path);
    return 0;
}

static vfs_ops_t dev_ops = {
    dev_vfs_open, dev_vfs_close, dev_vfs_read, dev_vfs_write,
    dev_vfs_stat, 0, 0, 0
};

void vfs_init(void) {
    kmemset(fd_table, 0, sizeof(fd_table));
    kmemset(mounts,   0, sizeof(mounts));
    kmemset(mem_open, 0, sizeof(mem_open));
    kmemset(disk_open,0, sizeof(disk_open));
    num_mounts = 0;

    pipe_init();

    vfs_mount("/mem",  &mem_ops);
    vfs_mount("/disk", &disk_ops);
    vfs_mount("/dev",  &dev_ops);
}

int vfs_mount(const char *prefix, vfs_ops_t *ops) {
    if (num_mounts >= VFS_MAX_MOUNTS) return -1;
    kstrcpy(mounts[num_mounts].prefix, prefix);
    mounts[num_mounts].ops    = ops;
    mounts[num_mounts].active = 1;
    num_mounts++;
    return 0;
}

void vfs_init_stdio(void) {

    for (int i = 0; i < 3; i++) {
        fd_table[i].used      = 1;
        fd_table[i].type      = VFS_DEV;
        fd_table[i].mount_idx = find_mount("/dev", 0);
        fd_table[i].fd_data   = i;
        fd_table[i].flags     = (i == 0) ? O_RDONLY : O_WRONLY;
    }
}

static int alloc_fd(void) {
    for (int i = 3; i < VFS_MAX_FD; i++)
        if (!fd_table[i].used) return i;
    return -1;
}

int vfs_open(const char *path, int flags) {

    char resolved[VFS_MAX_PATH];
    if (path[0] == '/') {
        kstrcpy(resolved, path);
    } else {

        kstrcpy(resolved, "/disk/");
        kstrcat(resolved, path);
    }

    const char *local = 0;
    int midx = find_mount(resolved, &local);
    vfs_stat_t probe_st;

    if (midx < 0 || (mounts[midx].ops->stat &&
        mounts[midx].ops->stat(local, &probe_st) < 0
        && !(flags & O_CREAT))) {

        char mem_path[VFS_MAX_PATH] = "/mem/";
        kstrcat(mem_path, path[0]=='/'?path+1:path);
        int midx2 = find_mount(mem_path, &local);
        if (midx2 >= 0 && mounts[midx2].ops->stat &&
            mounts[midx2].ops->stat(local, &probe_st) == 0) {
            midx = midx2;
            kstrcpy(resolved, mem_path);
            find_mount(resolved, &local);
        }
    }

    if (midx < 0 || !mounts[midx].ops->open) return -1;

    int data = mounts[midx].ops->open(local, flags);
    if (data < 0) return -1;

    int fd = alloc_fd();
    if (fd < 0) { mounts[midx].ops->close(data); return -1; }

    fd_table[fd].used      = 1;
    fd_table[fd].flags     = flags;
    fd_table[fd].pos       = 0;
    fd_table[fd].type      = VFS_FILE;
    fd_table[fd].mount_idx = midx;
    fd_table[fd].fd_data   = data;
    fd_table[fd].pipe_id   = -1;
    return fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].used) return -1;
    if (fd_table[fd].type == VFS_PIPE) {
        if (fd_table[fd].pipe_end == 0) pipe_close_read(fd_table[fd].pipe_id);
        else                            pipe_close_write(fd_table[fd].pipe_id);
        fd_table[fd].used = 0;
        return 0;
    }
    int midx = fd_table[fd].mount_idx;
    if (midx >= 0 && mounts[midx].ops->close)
        mounts[midx].ops->close(fd_table[fd].fd_data);
    fd_table[fd].used = 0;
    return 0;
}

int vfs_read(int fd, void *buf, uint32_t len) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].used) return -1;
    if (fd_table[fd].type == VFS_PIPE)
        return pipe_read(fd_table[fd].pipe_id, buf, len);
    int midx = fd_table[fd].mount_idx;
    if (midx < 0 || !mounts[midx].ops->read) return -1;
    int n = mounts[midx].ops->read(fd_table[fd].fd_data, buf, len);
    if (n > 0) fd_table[fd].pos += (uint32_t)n;
    return n;
}

int vfs_write(int fd, const void *buf, uint32_t len) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].used) return -1;
    if (fd_table[fd].type == VFS_PIPE)
        return pipe_write(fd_table[fd].pipe_id, buf, len);
    int midx = fd_table[fd].mount_idx;
    if (midx < 0 || !mounts[midx].ops->write) return -1;
    int n = mounts[midx].ops->write(fd_table[fd].fd_data, buf, len);
    if (n > 0) fd_table[fd].pos += (uint32_t)n;
    return n;
}

int vfs_stat(const char *path, vfs_stat_t *st) {
    char resolved[VFS_MAX_PATH];
    if (path[0]=='/') kstrcpy(resolved, path);
    else { kstrcpy(resolved, "/disk/"); kstrcat(resolved, path); }
    const char *local = 0;
    int midx = find_mount(resolved, &local);
    if (midx < 0 || !mounts[midx].ops->stat) return -1;
    return mounts[midx].ops->stat(local, st);
}

int vfs_readdir(const char *path, char *buf, uint32_t sz) {
    char resolved[VFS_MAX_PATH];
    if (path[0]=='/') kstrcpy(resolved, path);
    else { kstrcpy(resolved, cwd); kstrcat(resolved, "/"); kstrcat(resolved, path); }
    const char *local = 0;
    int midx = find_mount(resolved, &local);
    if (midx < 0 || !mounts[midx].ops->readdir) { buf[0]=0; return 0; }
    return mounts[midx].ops->readdir(local, buf, sz);
}

int vfs_unlink(const char *path) {
    char resolved[VFS_MAX_PATH];
    if (path[0]=='/') kstrcpy(resolved, path);
    else { kstrcpy(resolved, "/disk/"); kstrcat(resolved, path); }
    const char *local = 0;
    int midx = find_mount(resolved, &local);
    if (midx < 0 || !mounts[midx].ops->unlink) return -1;
    return mounts[midx].ops->unlink(local);
}

/* Real chmod, not fabricated: toggles the actual on-disk FAT12 read-only
   attribute bit. Only /disk has this (FAT12's dirent.attr byte) - the
   in-memory /mem fs and /dev have no on-disk representation to attach a
   permission bit to, so this only works for /disk paths.
   Mode syntax: a leading '-' always means "remove write" (read-only),
   regardless of what follows - a bare "if the mode string contains a 'w'
   anywhere" check would treat "-w" the same as "+w" (both contain 'w'),
   which is backwards. A leading '+' means "add write". Anything else is
   treated as an absolute octal-style mode (e.g. "644"/"755" writable,
   "444" read-only) via a bare presence check. */
int vfs_chmod(const char *path, const char *mode) {
    char resolved[VFS_MAX_PATH];
    if (path[0]=='/') kstrcpy(resolved, path);
    else { kstrcpy(resolved, "/disk/"); kstrcat(resolved, path); }
    if (kstrncmp(resolved, "/disk/", 6) != 0) return -1;

    int make_writable;
    if (mode[0] == '-') {
        make_writable = 0;
    } else if (mode[0] == '+') {
        make_writable = 1;
    } else {
        make_writable = 0;
        for (const char *p = mode; *p; p++) {
            if (*p=='w' || *p=='2' || *p=='6' || *p=='7') { make_writable = 1; break; }
        }
    }
    return fat12_set_readonly(resolved + 6, make_writable ? 0 : 1);
}

int vfs_getcwd(char *buf, uint32_t sz) {
    kstrncpy(buf, cwd, sz-1); buf[sz-1]=0;
    return (int)kstrlen(buf);
}

int vfs_chdir(const char *path) {
    if (path[0]=='/') kstrncpy(cwd, path, VFS_MAX_PATH-1);
    else { kstrcat(cwd, "/"); kstrcat(cwd, path); }
    return 0;
}

int vfs_pipe(int fds[2]) {
    int id = pipe_create();
    if (id < 0) return -1;

    int r = alloc_fd();
    if (r < 0) { pipe_close_read(id); pipe_close_write(id); return -1; }
    fd_table[r].used=1;
    int w = alloc_fd();
    if (w < 0) { fd_table[r].used=0; pipe_close_read(id); pipe_close_write(id); return -1; }

    fd_table[r].used=1; fd_table[r].type=VFS_PIPE;
    fd_table[r].pipe_id=id; fd_table[r].pipe_end=0;
    fd_table[r].mount_idx=-1; fd_table[r].flags=O_RDONLY;

    fd_table[w].used=1; fd_table[w].type=VFS_PIPE;
    fd_table[w].pipe_id=id; fd_table[w].pipe_end=1;
    fd_table[w].mount_idx=-1; fd_table[w].flags=O_WRONLY;

    fds[0]=r; fds[1]=w;
    return 0;
}

int vfs_dup2(int oldfd, int newfd) {
    if (oldfd<0||oldfd>=VFS_MAX_FD||!fd_table[oldfd].used) return -1;
    if (newfd<0||newfd>=VFS_MAX_FD) return -1;
    if (newfd == oldfd) return newfd;
    if (fd_table[newfd].used) vfs_close(newfd);
    fd_table[newfd] = fd_table[oldfd];
    if (fd_table[newfd].type == VFS_PIPE) {
        if (fd_table[newfd].pipe_end == 0) pipe_dup_read(fd_table[newfd].pipe_id);
        else                               pipe_dup_write(fd_table[newfd].pipe_id);
    }
    return newfd;
}

int vfs_isatty(int fd) {
    if (fd<0||fd>=VFS_MAX_FD||!fd_table[fd].used) return 0;
    return (fd_table[fd].type == VFS_DEV && fd_table[fd].fd_data < 3) ? 1 : 0;
}

vfs_fd_t *vfs_get_fd(int fd) {
    if (fd<0||fd>=VFS_MAX_FD||!fd_table[fd].used) return 0;
    return &fd_table[fd];
}