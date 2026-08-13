
#include "kumos_libc.h"

/* All of these need a "memory" clobber - without it, GCC doesn't know the
   int 0x80 call writes through a pointer argument (fds/buf/st) and is free
   to keep a stale cached copy of anything it thinks it already knows the
   value of, instead of reloading from memory afterward. kumos_libc.h's own
   _syscall() already has this; these were hand-duplicated here without it.
   Real, reproducible bug: sys_pipe() on a freshly-declared, compiler-known
   {-1,-1} array let GCC skip the reload and pass -1 to the very next
   sys_dup2() call, silently no-opping the pipe redirect (output leaked
   straight to the console instead of into the pipe) - only surfaced once
   run_pipeline()'s pipe-stage loop put pipefd[] in a shape (freshly
   re-declared each iteration) the optimizer could exploit; the single,
   outer-scope pipefd[] the old 2-stage-only code used happened not to
   trigger it. */
static inline int sys_pipe(int fds[2]) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(21),"b"(fds):"memory"); return r;
}
static inline int sys_dup2(int old, int nw) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(22),"b"(old),"c"(nw):"memory"); return r;
}
static inline int sys_getcwd(char *buf, uint32_t sz) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(23),"b"(buf),"c"(sz):"memory"); return r;
}
static inline int sys_chdir(const char *path) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(24),"b"(path):"memory"); return r;
}
static inline int sys_stat(const char *path, void *st) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(25),"b"(path),"c"(st):"memory"); return r;
}
static inline int sys_unlink(const char *path) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(26),"b"(path):"memory"); return r;
}
static inline int sys_chmod(const char *path, const char *mode) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(27),"b"(path),"c"(mode):"memory"); return r;
}
static inline int sys_isatty(int fd) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(29),"b"(fd):"memory"); return r;
}
static inline int sys_listdir(const char *path, char *buf, uint32_t sz) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(14),"b"(path),"c"(buf),"d"(sz):"memory"); return r;
}

typedef struct { uint32_t size; uint8_t type; char name[64]; } stat_t;

#define CMD_MAX   256
#define ARG_MAX    32
#define HIST_MAX   16
#define KUSH_VER  "1.0"

static char history[HIST_MAX][CMD_MAX];
static int  hist_count = 0;
static char cwd_buf[128];

static void hist_add(const char *cmd) {
    if (!*cmd) return;
    if (hist_count < HIST_MAX) strcpy(history[hist_count++], cmd);
    else {
        for (int i=0;i<HIST_MAX-1;i++) strcpy(history[i],history[i+1]);
        strcpy(history[HIST_MAX-1], cmd);
    }
}

static int split(char *line, char *argv[], int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max-1) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    argv[argc] = 0;
    return argc;
}

static int do_echo(char *argv[], int argc) {
    for (int i=1;i<argc;i++) {
        fputs(argv[i]);
        if (i<argc-1) putchar(' ');
    }
    putchar('\n');
    return 0;
}

static int do_ls(char *argv[], int argc) {
    const char *path = argc>1 ? argv[1] : cwd_buf;
    char buf[2048];
    int n = sys_listdir(path, buf, sizeof(buf));
    if (n <= 0) { printf("ls: nothing in %s\n", path); return 1; }
    buf[n] = 0;

    char *p = buf;
    int col = 0;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        printf("  %-18s", p);
        if (++col % 3 == 0) putchar('\n');
        p = nl ? nl+1 : p+strlen(p);
    }
    if (col % 3 != 0) putchar('\n');
    return 0;
}

static int do_cat(char *argv[], int argc) {
    if (argc < 2) { fputs("cat: need filename\n"); return 1; }
    int fd = open(argv[1]);
    if (fd < 0) { printf("cat: %s: not found\n", argv[1]); return 1; }
    char buf[512]; int n;
    while ((n = fread(fd, buf, sizeof(buf)-1)) > 0) {
        buf[n]=0;
        write(buf, (uint32_t)n);
    }
    close(fd);
    return 0;
}

static int do_wc(char *argv[], int argc) {
    (void)argv; (void)argc;
    char buf[512]; int n;
    uint32_t lines=0, words=0, bytes=0;
    int in_word=0;
    while ((n = fread(0, buf, sizeof(buf))) > 0) {
        bytes += (uint32_t)n;
        for (int i=0;i<n;i++) {
            if (buf[i]=='\n') lines++;
            if (buf[i]==' '||buf[i]=='\t'||buf[i]=='\n') in_word=0;
            else if (!in_word) { in_word=1; words++; }
        }
    }
    printf("%6u %6u %6u\n", lines, words, bytes);
    return 0;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}

static int do_sort(char *argv[], int argc) {
    (void)argv; (void)argc;
    char *lines[512]; int nlines=0;
    static char buf[32768]; int total=0; int n;
    while ((n = fread(0, buf+total, (uint32_t)(sizeof(buf)-total-1))) > 0) total+=n;
    buf[total]=0;
    char *p=buf;
    while (*p && nlines<512) {
        char *nl=strchr(p,'\n');
        if (nl) *nl=0;
        lines[nlines++]=p;
        p = nl ? nl+1 : p+strlen(p);
    }
    qsort(lines,(size_t)nlines,sizeof(char*),cmp_str);
    for (int i=0;i<nlines;i++) { fputs(lines[i]); putchar('\n'); }
    return 0;
}

static int do_uniq(char *argv[], int argc) {
    (void)argv; (void)argc;
    static char all[32768]; int total=0; int n;
    while ((n = fread(0, all+total, (uint32_t)(sizeof(all)-total-1))) > 0) total+=n;
    all[total]=0;
    char prev[512]; prev[0]=0;
    char *p=all;
    while (*p) {
        char *nl=strchr(p,'\n'); if (nl) *nl=0;
        if (strcmp(p,prev)!=0) {
            fputs(p); putchar('\n');
            strncpy(prev,p,511);
        }
        p = nl ? nl+1 : p+strlen(p);
    }
    return 0;
}

/* accepts both "-n N" (two args) and "-N" (bundled), like real head/tail */
static int parse_n_flag(char *argv[], int argc, int *fi_out) {
    int n = 10, fi = 1;
    if (argc>2 && !strcmp(argv[1],"-n")) { n = atoi(argv[2]); fi = 3; }
    else if (argc>1 && argv[1][0]=='-' && argv[1][1]) { n = atoi(argv[1]+1); fi = 2; }
    *fi_out = fi;
    return n;
}

static int do_head(char *argv[], int argc) {
    int fi; int n = parse_n_flag(argv, argc, &fi);
    int fd = 0;
    if (argc>fi) { fd = open(argv[fi]); if (fd<0) { printf("head: %s: not found\n", argv[fi]); return 1; } }
    static char buf[8192]; int total=0; int r;
    while (total<(int)sizeof(buf)-1 && (r=fread(fd,buf+total,(uint32_t)(sizeof(buf)-total-1)))>0) total+=r;
    buf[total]=0;
    if (fd) close(fd);
    char *p=buf; int lines=0;
    while (*p && lines<n) {
        char *nl=strchr(p,'\n');
        if (nl) *nl=0;
        fputs(p); putchar('\n');
        p = nl ? nl+1 : p+strlen(p);
        lines++;
    }
    return 0;
}

static int do_tail(char *argv[], int argc) {
    int fi; int n = parse_n_flag(argv, argc, &fi);
    int fd = 0;
    if (argc>fi) { fd = open(argv[fi]); if (fd<0) { printf("tail: %s: not found\n", argv[fi]); return 1; } }
    static char buf[8192]; int total=0; int r;
    while (total<(int)sizeof(buf)-1 && (r=fread(fd,buf+total,(uint32_t)(sizeof(buf)-total-1)))>0) total+=r;
    buf[total]=0;
    if (fd) close(fd);
    char *lines[512]; int nlines=0;
    char *p=buf;
    while (*p && nlines<512) {
        char *nl=strchr(p,'\n');
        if (nl) *nl=0;
        lines[nlines++]=p;
        p = nl ? nl+1 : p+strlen(p);
    }
    int start = nlines>n ? nlines-n : 0;
    for (int i=start;i<nlines;i++) { fputs(lines[i]); putchar('\n'); }
    return 0;
}

static int do_grep(char *argv[], int argc) {
    if (argc < 2) { fputs("grep: grep <pattern> [file]\n"); return 1; }
    const char *pat = argv[1];
    int fd = 0;
    if (argc > 2) { fd = open(argv[2]); if (fd<0) { printf("grep: %s: not found\n", argv[2]); return 1; } }
    static char buf[8192]; int total=0; int r;
    while (total<(int)sizeof(buf)-1 && (r=fread(fd,buf+total,(uint32_t)(sizeof(buf)-total-1)))>0) total+=r;
    buf[total]=0;
    if (fd) close(fd);
    char *p=buf;
    int found = 0;
    while (*p) {
        char *nl=strchr(p,'\n');
        if (nl) *nl=0;
        if (strstr(p,pat)) { fputs(p); putchar('\n'); found = 1; }
        p = nl ? nl+1 : p+strlen(p);
    }
    /* real grep exit status: 0 if a match was found, 1 if not - this is
       what makes `if grep pat file` (or in a script's if/while condition)
       actually mean something instead of always being true. */
    return found ? 0 : 1;
}

static int do_cp(char *argv[], int argc) {
    if (argc < 3) { fputs("cp: cp <src> <dst>\n"); return 1; }
    int src = open(argv[1]);
    if (src < 0) { printf("cp: %s: not found\n", argv[1]); return 1; }

    char dst_path[128] = "/disk/";
    strcat(dst_path, argv[2]);
    int dst = _syscall(SYS_OPEN, (int)dst_path, O_WRONLY|O_CREAT|O_TRUNC, 0);
    if (dst < 0) { close(src); printf("cp: can't create %s\n",argv[2]); return 1; }
    char buf[512]; int n;
    while ((n=fread(src,buf,512))>0) _syscall(SYS_FWRITE,dst,(int)buf,(int)n);
    close(src); close(dst);
    printf("Copied %s -> %s\n", argv[1], argv[2]);
    return 0;
}

static int do_mv(char *argv[], int argc) {
    if (argc < 3) { fputs("mv: mv <src> <dst>\n"); return 1; }
    int src = open(argv[1]);
    if (src < 0) { printf("mv: %s: not found\n", argv[1]); return 1; }

    char dst_path[128] = "/disk/";
    strcat(dst_path, argv[2]);
    int dst = _syscall(SYS_OPEN, (int)dst_path, O_WRONLY|O_CREAT|O_TRUNC, 0);
    if (dst < 0) { close(src); printf("mv: can't create %s\n", argv[2]); return 1; }
    char buf[512]; int n;
    while ((n=fread(src,buf,512))>0) _syscall(SYS_FWRITE,dst,(int)buf,(int)n);
    close(src); close(dst);

    if (sys_unlink(argv[1]) != 0) { printf("mv: copied but couldn't remove %s\n", argv[1]); return 1; }
    printf("Moved %s -> %s\n", argv[1], argv[2]);
    return 0;
}

static int do_rm(char *argv[], int argc) {
    if (argc < 2) { fputs("rm: need filename\n"); return 1; }
    if (sys_unlink(argv[1]) == 0) { printf("Deleted %s\n", argv[1]); return 0; }
    printf("rm: %s: failed\n", argv[1]); return 1;
}

static int do_cd(char *argv[], int argc) {
    const char *path = argc>1 ? argv[1] : "/disk";
    if (sys_chdir(path)==0) { sys_getcwd(cwd_buf,sizeof(cwd_buf)); return 0; }
    printf("cd: %s: not found\n", path); return 1;
}

static int do_pwd(char *argv[], int argc) {
    (void)argv;(void)argc;
    sys_getcwd(cwd_buf, sizeof(cwd_buf));
    puts(cwd_buf);
    return 0;
}

static int do_history(char *argv[], int argc) {
    (void)argv;(void)argc;
    for (int i=0;i<hist_count;i++) printf("  %2d  %s\n", i+1, history[i]);
    return 0;
}

static int do_stat(char *argv[], int argc) {
    if (argc<2){fputs("stat: need filename\n");return 1;}
    stat_t st;
    if (sys_stat(argv[1], &st)<0){printf("stat: %s: not found\n",argv[1]);return 1;}
    printf("  File: %s\n  Size: %u bytes\n  Type: %s\n",
           st.name, st.size,
           st.type==1?"file":st.type==2?"dir":st.type==3?"device":"pipe");
    return 0;
}

/* Toggles the real on-disk FAT12 read-only attribute bit (src/fat12.c),
   not a fabricated permission model - the only per-file access control
   this filesystem format actually has, and only /disk has it (no on-disk
   representation on /mem or /dev to attach a bit to). */
static int do_chmod(char *argv[], int argc) {
    if (argc < 3) { fputs("chmod: chmod <mode> <file>\n"); return 1; }
    if (sys_chmod(argv[2], argv[1]) == 0) { printf("chmod: %s: %s\n", argv[2], argv[1]); return 0; }
    printf("chmod: %s: failed (only /disk files have a real read-only bit)\n", argv[2]);
    return 1;
}

/* Non-recursive filename search (ls itself is non-recursive too - there's
   no directory-tree-walk API in the vfs layer yet to build a real
   recursive find on top of). */
static int do_find(char *argv[], int argc) {
    if (argc < 2) { fputs("find: find [dir] <name>\n"); return 1; }
    const char *pat  = argv[argc-1];
    const char *path = argc>2 ? argv[1] : cwd_buf;

    char buf[2048];
    int n = sys_listdir(path, buf, sizeof(buf));
    if (n <= 0) return 0;
    buf[n] = 0;

    char *p = buf; int found = 0;
    while (*p) {
        char *nl = strchr(p,'\n');
        if (nl) *nl = 0;
        if (strstr(p, pat)) { printf("%s/%s\n", path, p); found = 1; }
        p = nl ? nl+1 : p+strlen(p);
    }
    if (!found) printf("find: no matches for '%s' in %s\n", pat, path);
    return 0;
}

static int do_du(char *argv[], int argc) {
    const char *path = argc>1 ? argv[1] : cwd_buf;

    stat_t st;
    if (sys_stat(path, &st) == 0 && st.type != 2) {
        printf("%6u  %s\n", st.size, path);
        return 0;
    }

    char buf[2048];
    int n = sys_listdir(path, buf, sizeof(buf));
    if (n <= 0) { printf("du: %s: not found\n", path); return 1; }
    buf[n] = 0;

    char *p = buf; uint32_t total = 0;
    while (*p) {
        char *nl = strchr(p,'\n');
        if (nl) *nl = 0;
        char full[192]; strcpy(full, path);
        if (full[strlen(full)-1] != '/') strcat(full, "/");
        strcat(full, p);
        stat_t est;
        if (sys_stat(full, &est) == 0) { printf("%6u  %s\n", est.size, full); total += est.size; }
        p = nl ? nl+1 : p+strlen(p);
    }
    printf("%6u  total\n", total);
    return 0;
}

/* Basic sed s/old/new/ [g] substitution - one expression, one file (or
   stdin), no full regex, no other sed commands. */
static int do_sed(char *argv[], int argc) {
    if (argc < 2 || argv[1][0] != 's' || argv[1][1] != '/') { fputs("sed: sed s/old/new/[g] [file]\n"); return 1; }

    char expr[192]; strncpy(expr, argv[1]+2, sizeof(expr)-1); expr[sizeof(expr)-1]=0;
    char *old = expr;
    char *sep2 = strchr(old, '/');
    if (!sep2) { fputs("sed: bad expression, need s/old/new/\n"); return 1; }
    *sep2 = 0;
    char *new_ = sep2+1;
    char *sep3 = strchr(new_, '/');
    int global = 0;
    if (sep3) { global = (sep3[1]=='g'); *sep3 = 0; }
    int oldlen = (int)strlen(old);

    int fd = 0;
    if (argc > 2) { fd = open(argv[2]); if (fd<0) { printf("sed: %s: not found\n", argv[2]); return 1; } }
    static char buf[8192]; int total=0; int r;
    while (total<(int)sizeof(buf)-1 && (r=fread(fd,buf+total,(uint32_t)(sizeof(buf)-total-1)))>0) total+=r;
    buf[total]=0;
    if (fd) close(fd);

    char *p = buf;
    while (*p) {
        char *nl = strchr(p,'\n');
        if (nl) *nl = 0;

        char outline[512]; int oi=0; int replaced=0;
        char *q = p;
        while (*q && oi < (int)sizeof(outline)-1) {
            if ((!replaced || global) && oldlen>0 && strncmp(q, old, (uint32_t)oldlen)==0) {
                for (const char *rp=new_; *rp && oi<(int)sizeof(outline)-1; rp++) outline[oi++]=*rp;
                q += oldlen;
                replaced = 1;
            } else {
                outline[oi++] = *q++;
            }
        }
        outline[oi] = 0;
        fputs(outline); putchar('\n');
        p = nl ? nl+1 : p+strlen(p);
    }
    return 0;
}

static int do_write(char *argv[], int argc) {
    if (argc<3){fputs("write: write <file> <content>\n");return 1;}

    char content[512]; content[0]=0;
    for (int i=2;i<argc;i++){strcat(content,argv[i]);if(i<argc-1)strcat(content," ");}
    strcat(content,"\n");
    char path[128]="/disk/"; strcat(path,argv[1]);
    int fd=_syscall(SYS_OPEN,(int)path,O_WRONLY|O_CREAT|O_TRUNC,0);
    if(fd<0){printf("write: can't create %s\n",argv[1]);return 1;}
    _syscall(SYS_FWRITE,fd,(int)content,(int)strlen(content));
    close(fd);
    printf("Written to %s\n",argv[1]);
    return 0;
}

static int do_date(char *argv[], int argc) {
    (void)argv;(void)argc;
    print_time();
    putchar('\n');
    return 0;
}

static int do_kill(char *argv[], int argc) {
    if (argc<2){fputs("kill: kill <pid> [sig]\n");return 1;}
    int pid=atoi(argv[1]);
    int sig=(argc>2)?atoi(argv[2]):15;
    int r=sys_kill(pid,sig);
    if(r<0){printf("kill: pid %d not found\n",pid);return 1;}
    printf("Signal %d sent to PID %d\n",sig,pid);
    return 0;
}

static int do_cat_proc(const char *path) {
    int fd=_syscall(SYS_OPEN,(int)path,O_RDONLY,0);
    if(fd<0){printf("cat: %s: not found\n",path);return 1;}
    char buf[2048]; int n=fread(fd,buf,sizeof(buf)-1);
    if(n>0){buf[n]=0;fputs(buf);}
    close(fd);
    return 0;
}

static int do_help(char *argv[], int argc) {
    (void)argv;(void)argc;
    fputs("\n  kush v" KUSH_VER " — KumOS Shell\n\n");
    fputs("  Built-ins:\n");
    fputs("    help              - This help\n");
    fputs("    ls [path]         - List files\n");
    fputs("    cat <file>        - Print file\n");
    fputs("    head [-n N] [f]   - First N lines (default 10)\n");
    fputs("    tail [-n N] [f]   - Last N lines (default 10)\n");
    fputs("    grep <pat> [f]    - Print matching lines\n");
    fputs("    cp <src> <dst>    - Copy file\n");
    fputs("    mv <src> <dst>    - Move/rename file\n");
    fputs("    set               - List shell variables\n");
    fputs("    NAME=value        - Set a shell variable ($NAME to use it)\n");
    fputs("    alias             - List aliases\n");
    fputs("    alias name=value  - Define an alias\n");
    fputs("    rm <file>         - Delete file\n");
    fputs("    write <f> <data>  - Write text to file\n");
    fputs("    cd [path]         - Change directory\n");
    fputs("    pwd               - Print working directory\n");
    fputs("    stat <file>       - File info\n");
    fputs("    chmod <mode> <f>  - Toggle real FAT12 read-only bit (+w/-w or e.g 644/444)\n");
    fputs("    find [dir] <name> - Search filenames (non-recursive)\n");
    fputs("    du [path]         - Disk usage\n");
    fputs("    sed s/old/new/[g] [f] - Stream substitution\n");
    fputs("    date              - Current date/time\n");
    fputs("    history           - Command history\n");
    fputs("    !!  !N            - Repeat last / Nth history entry\n");
    fputs("    exit [code]       - Exit shell\n");
    fputs("    kill <pid> [sig]  - Send signal\n\n");
    fputs("  /proc files:\n");
    fputs("    cat /proc/meminfo  cat /proc/ps\n");
    fputs("    cat /proc/uptime   cat /proc/net\n\n");
    fputs("  ELF programs (from disk):\n");
    fputs("    HELLO.ELF         - Hello world\n");
    fputs("    COUNTER.ELF       - Counter demo\n");
    fputs("    CAT.ELF           - File reader\n");
    fputs("    SYSINFO.ELF       - System info\n\n");
    fputs("  Pipes:  cmd1 | cmd2 | cmd3 (any number of stages)\n\n");
    fputs("  Scripts (source <file> or . <file>), one keyword per own line:\n");
    fputs("    if <cmd> / then / ... / else / ... / fi\n");
    fputs("    for VAR in a b c / do / ... / done\n");
    fputs("    while <cmd> / do / ... / done\n\n");
    return 0;
}

static int do_exec(const char *cmd) {

    char upper[64]; int i=0;
    while(cmd[i]&&i<60){char c=cmd[i];if(c>='a'&&c<='z')c-=32;upper[i++]=c;}
    upper[i]=0;

    if (!strchr(upper, '.')) { strcat(upper, ".ELF"); }

    /* NOTE: this replaces kush itself (real execve, no fork). A real
       fork()+exec()+wait() was tried again this round and got much
       further than before: the round12/13 bug (paging_clone_dir()
       aliasing the page directory entry user ELF code loads into, PDE 1
       at virt 0x400000) is genuinely fixed now - see src/paging.c's
       comments on paging_clone_dir()/pte_ptr() - and a real, separate
       reentrancy bug in sched_yield() (a premature `sti` before the
       actual stack-pointer swap in switch_context(), letting a nested
       timer tick corrupt a mid-switch task's register frame) is also
       fixed, verified by serial-tracing the corrupted invalid-opcode
       panic it caused and watching it disappear.

       But a THIRD, deeper bug remains: after sched_waitpid()'s busy-wait
       loop yields many times (waiting for the forked child to exit) and
       finally returns, kush's own return-to-ring-3 (the plain `iret` at
       the end of isr128) lands in garbage - EIP and ESP both end up as
       stack-address-looking values, not the correct saved ones. Confirmed
       via serial tracing that the syscall's own saved register frame
       reads back correct right up to the very last C statement before
       isr128's fixed pop/iret sequence runs (with interrupts confirmed
       off throughout, and even with -O0 forced on the entire call chain
       to rule out a compiler bug) - yet the actual iret still picks up
       wrong values. Root cause not found this round. Left as plain
       sys_exec() rather than ship a reproducible crash; see
       project-kumos-round14 memory for the full debugging trail. */
    int r = sys_exec(upper);
    if (r < 0) { printf("kush: %s: not found\n", cmd); return 127; }
    return r;
}

static int run_script(const char *filename);
static void do_set(void);

static int dispatch_one(char *argv[], int argc) {
    const char *cmd = argv[0];
    if (!strcmp(cmd,"exit")){exit(argc>1?atoi(argv[1]):0);}
    if (!strcmp(cmd,"set")) { do_set(); return 0; }
    if (!strcmp(cmd,"source")||!strcmp(cmd,".")) {
        if(argc>1) return run_script(argv[1]);
        puts("source: need filename"); return 1;
    }
    if (!strcmp(cmd,"help"))    return do_help(argv,argc);
    if (!strcmp(cmd,"ls"))      return do_ls(argv,argc);
    if (!strcmp(cmd,"cat"))     return do_cat(argv,argc);
    if (!strcmp(cmd,"wc"))      return do_wc(argv,argc);
    if (!strcmp(cmd,"sort"))    return do_sort(argv,argc);
    if (!strcmp(cmd,"uniq"))    return do_uniq(argv,argc);
    if (!strcmp(cmd,"head"))    return do_head(argv,argc);
    if (!strcmp(cmd,"tail"))    return do_tail(argv,argc);
    if (!strcmp(cmd,"grep"))    return do_grep(argv,argc);
    if (!strcmp(cmd,"cp"))      return do_cp(argv,argc);
    if (!strcmp(cmd,"mv"))      return do_mv(argv,argc);
    if (!strcmp(cmd,"rm"))      return do_rm(argv,argc);
    if (!strcmp(cmd,"cd"))      return do_cd(argv,argc);
    if (!strcmp(cmd,"pwd"))     return do_pwd(argv,argc);
    if (!strcmp(cmd,"stat"))    return do_stat(argv,argc);
    if (!strcmp(cmd,"chmod"))   return do_chmod(argv,argc);
    if (!strcmp(cmd,"find"))    return do_find(argv,argc);
    if (!strcmp(cmd,"du"))      return do_du(argv,argc);
    if (!strcmp(cmd,"sed"))     return do_sed(argv,argc);
    if (!strcmp(cmd,"echo"))    return do_echo(argv,argc);
    if (!strcmp(cmd,"date"))    return do_date(argv,argc);
    if (!strcmp(cmd,"history")) return do_history(argv,argc);
    if (!strcmp(cmd,"write"))   return do_write(argv,argc);
    if (!strcmp(cmd,"kill"))    return do_kill(argv,argc);
    if (!strcmp(cmd,"clear"))   { fputs("\033[2J\033[H"); return 0; }
    return do_exec(cmd);
}

/* Strips >, >>, < tokens (and their filename argument) out of argv,
   compacting the remaining args in place. Requires whitespace around the
   operator, same as split()'s space-only tokenizing - "cmd>file" with no
   spaces isn't recognized, only "cmd > file". */
static void parse_redir(char *argv[], int *argc, char **out_file, int *append, char **in_file) {
    int j = 0;
    for (int i = 0; i < *argc; i++) {
        if (!strcmp(argv[i], ">") && i+1 < *argc)       { *out_file = argv[++i]; *append = 0; }
        else if (!strcmp(argv[i], ">>") && i+1 < *argc) { *out_file = argv[++i]; *append = 1; }
        else if (!strcmp(argv[i], "<") && i+1 < *argc)  { *in_file  = argv[++i]; }
        else argv[j++] = argv[i];
    }
    argv[j] = 0;
    *argc = j;
}

/* Shell variables: VAR=value assignment + $VAR expansion. This is the
   scoped-down piece of "scripting" that landed - real control flow
   (if/for/while) would need a proper parser/interpreter and didn't fit
   in the time available this round. */
#define KUSH_MAX_VARS 16
#define KUSH_VAR_NAME 32
#define KUSH_VAR_VAL  192
static char var_names[KUSH_MAX_VARS][KUSH_VAR_NAME];
static char var_vals[KUSH_MAX_VARS][KUSH_VAR_VAL];
static int  nvars = 0;

static int is_var_char(char c) { return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'; }

static const char *var_get(const char *name, int len) {
    for (int i=0;i<nvars;i++) {
        int j=0; while (j<len && var_names[i][j]==name[j]) j++;
        if (j==len && var_names[i][j]==0) return var_vals[i];
    }
    return 0;
}

static void var_set(const char *name, int len, const char *val) {
    if (len >= KUSH_VAR_NAME) len = KUSH_VAR_NAME-1;
    for (int i=0;i<nvars;i++) {
        int j=0; while (j<len && var_names[i][j]==name[j]) j++;
        if (j==len && var_names[i][j]==0) { strncpy(var_vals[i], val, KUSH_VAR_VAL-1); var_vals[i][KUSH_VAR_VAL-1]=0; return; }
    }
    if (nvars < KUSH_MAX_VARS) {
        strncpy(var_names[nvars], name, len); var_names[nvars][len]=0;
        strncpy(var_vals[nvars], val, KUSH_VAR_VAL-1); var_vals[nvars][KUSH_VAR_VAL-1]=0;
        nvars++;
    }
}

static void do_set(void) {
    for (int i=0;i<nvars;i++) printf("%s=%s\n", var_names[i], var_vals[i]);
}

/* NAME=value with no spaces around '=' - consumes the whole line as an
   assignment (not dispatched as a command) if it matches. */
static int try_assign(const char *line) {
    const char *p = line;
    if (!(( *p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||*p=='_')) return 0;
    const char *start = p;
    while (is_var_char(*p)) p++;
    if (*p != '=' || p == start) return 0;
    int len = (int)(p-start);
    var_set(start, len, p+1);
    return 1;
}

static void expand_vars(const char *in, char *out, int outmax) {
    int oi = 0;
    for (int i=0; in[i] && oi<outmax-1; ) {
        if (in[i]=='$' && is_var_char(in[i+1]) && !(in[i+1]>='0'&&in[i+1]<='9')) {
            int j = i+1;
            while (is_var_char(in[j])) j++;
            const char *val = var_get(in+i+1, j-(i+1));
            if (val) for (int k=0; val[k] && oi<outmax-1; k++) out[oi++] = val[k];
            i = j;
        } else {
            out[oi++] = in[i++];
        }
    }
    out[oi] = 0;
}

#define KUSH_MAX_ALIAS 16
#define KUSH_ALIAS_NAME 16
#define KUSH_ALIAS_VAL  128
static char alias_names[KUSH_MAX_ALIAS][KUSH_ALIAS_NAME];
static char alias_vals[KUSH_MAX_ALIAS][KUSH_ALIAS_VAL];
static int  nalias = 0;

static const char *alias_get(const char *name) {
    for (int i=0;i<nalias;i++) if (!strcmp(alias_names[i],name)) return alias_vals[i];
    return 0;
}

static void alias_set(const char *name, const char *val) {
    for (int i=0;i<nalias;i++) {
        if (!strcmp(alias_names[i],name)) {
            strncpy(alias_vals[i],val,KUSH_ALIAS_VAL-1); alias_vals[i][KUSH_ALIAS_VAL-1]=0;
            return;
        }
    }
    if (nalias < KUSH_MAX_ALIAS) {
        strncpy(alias_names[nalias],name,KUSH_ALIAS_NAME-1); alias_names[nalias][KUSH_ALIAS_NAME-1]=0;
        strncpy(alias_vals[nalias],val,KUSH_ALIAS_VAL-1); alias_vals[nalias][KUSH_ALIAS_VAL-1]=0;
        nalias++;
    }
}

static void do_alias_list(void) {
    for (int i=0;i<nalias;i++) printf("alias %s='%s'\n", alias_names[i], alias_vals[i]);
}

/* "alias" with no args lists them, "alias name=value" defines one - handled
   as a whole-line special case (like try_assign) rather than a normal
   dispatch_one builtin, since it needs the raw unsplit line to capture a
   multi-word value after the '='. */
static int try_alias_cmd(const char *line) {
    if (strncmp(line,"alias",5) != 0) return 0;
    if (line[5] != 0 && line[5] != ' ') return 0;
    const char *rest = line+5;
    while (*rest==' ') rest++;
    if (!*rest) { do_alias_list(); return 1; }
    const char *eq = strchr(rest,'=');
    if (!eq) { fputs("alias: usage: alias name=value\n"); return 1; }
    char name[KUSH_ALIAS_NAME]; int nlen=(int)(eq-rest);
    if (nlen >= KUSH_ALIAS_NAME) nlen = KUSH_ALIAS_NAME-1;
    strncpy(name, rest, (size_t)nlen); name[nlen]=0;
    alias_set(name, eq+1);
    return 1;
}

/* Expands a leading alias reference by splicing its value in place of the
   first word - single pass, non-recursive (an alias whose value is itself
   an alias name won't chain, a deliberate simplification). */
static void expand_alias(char *line, int max) {
    char first[KUSH_ALIAS_NAME]; int fi=0;
    char *q = line;
    while (*q==' ') q++;
    while (*q && *q!=' ' && fi<KUSH_ALIAS_NAME-1) first[fi++]=*q++;
    first[fi]=0;
    const char *aval = alias_get(first);
    if (!aval) return;
    char combined[CMD_MAX];
    strncpy(combined, aval, CMD_MAX-1); combined[CMD_MAX-1]=0;
    strncat(combined, q, CMD_MAX-1-strlen(combined));
    strncpy(line, combined, (size_t)max-1); line[max-1]=0;
}

static int run_pipeline(char *line) {

    if (try_assign(line)) return 0;
    if (try_alias_cmd(line)) return 0;

    char expanded[CMD_MAX];
    expand_vars(line, expanded, CMD_MAX);
    expand_alias(expanded, CMD_MAX);
    line = expanded;

    char *parts[8]; int nparts = 0;
    char *p = line;
    parts[nparts++] = p;
    while (*p) {
        if (*p == '|') { *p++ = 0; parts[nparts++] = p; }
        else p++;
    }

    if (nparts == 1) {

        char *argv[ARG_MAX]; int argc = split(parts[0], argv, ARG_MAX);
        if (!argc) return 0;

        char *out_file = 0, *in_file = 0; int append = 0;
        parse_redir(argv, &argc, &out_file, &append, &in_file);
        if (!argc) return 0;

        /* vfs_dup2() only refcounts pipes, not regular files - closing
           the original fd right after dup2'ing it onto 1/0 (the way the
           pipe code below does) frees the underlying FAT12 file handle
           the dup'd descriptor still depends on, so every write after
           that silently goes nowhere (file gets created via O_CREAT but
           stays 0 bytes). Fix: keep the original fd open until *after*
           fd 1/0 have been restored to their saved values, so it's no
           longer aliased by anything when it's finally closed. */
        int out_fd = -1, in_fd = -1;
        int saved_out = -1, saved_in = -1;
        if (out_file) {
            char path[128] = "/disk/"; strcat(path, out_file);
            out_fd = _syscall(SYS_OPEN, (int)path, O_WRONLY|O_CREAT|(append?O_APPEND:O_TRUNC), 0);
            if (out_fd < 0) { printf("kush: can't open %s\n", out_file); return 1; }
            saved_out = sys_dup2(1, 12);
            sys_dup2(out_fd, 1);
        }
        if (in_file) {
            in_fd = open(in_file);
            if (in_fd < 0) {
                printf("kush: %s: not found\n", in_file);
                if (saved_out >= 0) { sys_dup2(saved_out, 1); close(saved_out); }
                if (out_fd >= 0) close(out_fd);
                return 1;
            }
            saved_in = sys_dup2(0, 13);
            sys_dup2(in_fd, 0);
        }

        int ret = dispatch_one(argv, argc);

        if (saved_out >= 0) { sys_dup2(saved_out, 1); close(saved_out); }
        if (saved_in  >= 0) { sys_dup2(saved_in,  0); close(saved_in);  }
        if (out_fd >= 0) close(out_fd);
        if (in_fd  >= 0) close(in_fd);
        return ret;
    }

    /* N-stage pipeline (a|b|c|...). Builtins run in-process (no fork), so
       each stage runs to full completion before the next one starts -
       stage i's whole output goes into a pipe, then stage i+1 reads it
       back out. Was hardcoded to exactly 2 stages (parts[0]/parts[1]),
       silently dropping anything past the second '|' - now loops over
       however many stages split() found (up to 8). */
    int prev_read = -1;
    int last_ret = 0;
    for (int i = 0; i < nparts; i++) {
        char *argv[ARG_MAX]; int argc = split(parts[i], argv, ARG_MAX);
        if (!argc) { if (prev_read >= 0) close(prev_read); return 0; }

        int is_last = (i == nparts - 1);
        int pipefd[2] = {-1, -1};
        if (!is_last && sys_pipe(pipefd) < 0) {
            fputs("pipe failed\n");
            if (prev_read >= 0) close(prev_read);
            return 1;
        }
        int saved_stdin = -1, saved_stdout = -1;
        if (prev_read >= 0) {
            saved_stdin = sys_dup2(0, 11);
            sys_dup2(prev_read, 0);
            close(prev_read);
        }
        if (!is_last) {
            saved_stdout = sys_dup2(1, 10);
            sys_dup2(pipefd[1], 1);
            close(pipefd[1]);
        }

        /* Routes through dispatch_one itself (not a hand-copied builtin
           subset) so every builtin works as a pipe stage automatically. */
        last_ret = dispatch_one(argv, argc);

        if (saved_stdout >= 0) { sys_dup2(saved_stdout, 1); close(saved_stdout); }
        if (saved_stdin  >= 0) { sys_dup2(saved_stdin,  0); close(saved_stdin);  }

        prev_read = is_last ? -1 : pipefd[0];
    }
    /* Real pipeline exit status: the last stage's, not always 0 - so
       `if a | b` (used heavily by scripts, e.g. `if cmd | grep pat`)
       actually reflects whether the pipeline succeeded. */
    return last_ret;
}

static void print_prompt(void) {
    sys_getcwd(cwd_buf, sizeof(cwd_buf));
    printf("\033[32mkush\033[0m:\033[36m%s\033[0m$ ", cwd_buf);
}

static void motd(void) {
    fputs("\n");
    fputs("  _  ___   _ ____  _   _\n");
    fputs(" | |/ / | | / ___|| | | |\n");
    fputs(" | ' /| | | \\___ \\| |_| |\n");
    fputs(" | . \\| |_| |___) |  _  |\n");
    fputs(" |_|\\_\\\\___/|____/|_| |_|\n");
    printf("\n  kush v%s - KumOS Shell  (type 'help')\n\n", KUSH_VER);
}

/* Real control flow for scripts: if/then/else/fi, for VAR in .../do/done,
   while <cmd>/do/done. Deliberately scoped down from real shell syntax -
   each keyword needs its own line (no "if cond; then" on one line) - to
   keep the block matcher a simple depth counter instead of a real
   tokenizer. Nesting works (any mix of if/for/while), nothing else does:
   no elif, no break/continue, no arithmetic, no functions. */
#define SCRIPT_MAX_LINES 200

static int is_block_open(const char *l) {
    return !strncmp(l,"if ",3) || !strcmp(l,"if") || !strncmp(l,"for ",4) || !strncmp(l,"while ",6);
}
static int is_block_close(const char *l) {
    return !strcmp(l,"fi") || !strcmp(l,"done");
}

/* Advances *idx to the matching fi/done for the block whose body starts
   at *idx (already past the opening line), landing ON that close line -
   used both for "skip the branch we're not taking" and "find where a
   loop body ends" duty. */
static void skip_block(char *lines[], int n, int *idx) {
    int depth = 1;
    while (*idx < n) {
        char *l = lines[*idx];
        if (is_block_open(l)) depth++;
        else if (is_block_close(l)) { depth--; if (!depth) return; }
        (*idx)++;
    }
}

/* Same, but also stops early on a same-depth "else" (for skipping an
   if-branch that wasn't taken, which needs to land on either). */
static void skip_to_else_or_close(char *lines[], int n, int *idx) {
    int depth = 1;
    while (*idx < n) {
        char *l = lines[*idx];
        if (is_block_open(l)) { depth++; (*idx)++; continue; }
        if (is_block_close(l)) { depth--; if (!depth) return; (*idx)++; continue; }
        if (depth==1 && !strcmp(l,"else")) return;
        (*idx)++;
    }
}

static int script_exec_block(char *lines[], int n, int *idx) {
    int ret = 0;
    while (*idx < n) {
        char *line = lines[*idx];
        if (!strcmp(line,"fi") || !strcmp(line,"done") || !strcmp(line,"else")) return ret;

        if (!strncmp(line,"if ",3) || !strcmp(line,"if")) {
            char cond[CMD_MAX]; strncpy(cond, line+2, CMD_MAX-1); cond[CMD_MAX-1]=0;
            char *c = cond; while (*c==' ') c++;
            (*idx)++;
            if (*idx<n && !strcmp(lines[*idx],"then")) (*idx)++;

            int cret = run_pipeline(c);
            if (cret == 0) ret = script_exec_block(lines, n, idx);
            else            skip_to_else_or_close(lines, n, idx);

            if (*idx<n && !strcmp(lines[*idx],"else")) {
                (*idx)++;
                if (cret != 0) ret = script_exec_block(lines, n, idx);
                else           skip_block(lines, n, idx);
            }
            if (*idx<n && !strcmp(lines[*idx],"fi")) (*idx)++;
            continue;
        }

        if (!strncmp(line,"while ",6)) {
            char cond[CMD_MAX]; strncpy(cond, line+6, CMD_MAX-1); cond[CMD_MAX-1]=0;
            (*idx)++;
            if (*idx<n && !strcmp(lines[*idx],"do")) (*idx)++;
            int body_start = *idx;

            int guard = 0; /* hard cap - a script bug shouldn't hang the shell forever */
            while (run_pipeline(cond) == 0 && guard < 10000) {
                int bi = body_start;
                ret = script_exec_block(lines, n, &bi);
                guard++;
            }
            *idx = body_start;
            skip_block(lines, n, idx);
            if (*idx<n && !strcmp(lines[*idx],"done")) (*idx)++;
            continue;
        }

        if (!strncmp(line,"for ",4)) {
            char rest[CMD_MAX]; strncpy(rest, line+4, CMD_MAX-1); rest[CMD_MAX-1]=0;
            char *fargv[ARG_MAX]; int fargc = split(rest, fargv, ARG_MAX);
            (*idx)++;
            if (*idx<n && !strcmp(lines[*idx],"do")) (*idx)++;
            int body_start = *idx;

            if (fargc < 3 || strcmp(fargv[1],"in") != 0) {
                fputs("kush: for: syntax: for VAR in item1 item2 ...\n");
            } else {
                for (int k=2; k<fargc; k++) {
                    var_set(fargv[0], (int)strlen(fargv[0]), fargv[k]);
                    int bi = body_start;
                    ret = script_exec_block(lines, n, &bi);
                }
            }
            *idx = body_start;
            skip_block(lines, n, idx);
            if (*idx<n && !strcmp(lines[*idx],"done")) (*idx)++;
            continue;
        }

        ret = run_pipeline(line);
        (*idx)++;
    }
    return ret;
}

static int run_script(const char *filename) {
    int fd = open(filename);
    if (fd < 0) { printf("kush: %s: not found\n", filename); return 1; }
    static char buf[4096]; int n = fread(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;

    static char *lines[SCRIPT_MAX_LINES];
    int nlines = 0;
    char *p = buf;
    while (*p && nlines < SCRIPT_MAX_LINES) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        while (*p == ' ' || *p == '\t') p++;
        char *end = p + strlen(p);
        while (end > p && (end[-1]==' '||end[-1]=='\t'||end[-1]=='\r')) *--end = 0;
        if (*p && *p != '#') lines[nlines++] = p;
        p = nl ? nl+1 : p+strlen(p);
    }

    int idx = 0;
    return script_exec_block(lines, nlines, &idx);
}

int main(void) {
    sys_getcwd(cwd_buf, sizeof(cwd_buf));
    if (!cwd_buf[0]) strcpy(cwd_buf, "/disk");

    motd();

    char line[CMD_MAX];
    int last_exit = 0;

    for (;;) {

        if (last_exit != 0) printf("[%d] ", last_exit);
        print_prompt();

        int n = read(line, CMD_MAX-1);
        if (n <= 0) continue;
        line[n] = 0;

        int len = (int)strlen(line);
        while (len>0 && (line[len-1]=='\n'||line[len-1]==' '||line[len-1]=='\r'))
            line[--len]=0;

        if (!*line) continue;

        if (line[0]=='!') {
            if (line[1]=='!' && line[2]==0) {
                if (!hist_count) { fputs("kush: no history\n"); continue; }
                strcpy(line, history[hist_count-1]);
                printf("%s\n", line);
            } else if (line[1]>='0'&&line[1]<='9') {
                int idx = atoi(line+1);
                if (idx<1 || idx>hist_count) { printf("kush: !%d: event not found\n", idx); continue; }
                strcpy(line, history[idx-1]);
                printf("%s\n", line);
            }
        }

        hist_add(line);
        last_exit = run_pipeline(line);
    }
}