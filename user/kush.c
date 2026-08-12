
#include "kumos_libc.h"

static inline int sys_pipe(int fds[2]) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(21),"b"(fds)); return r;
}
static inline int sys_dup2(int old, int nw) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(22),"b"(old),"c"(nw)); return r;
}
static inline int sys_getcwd(char *buf, uint32_t sz) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(23),"b"(buf),"c"(sz)); return r;
}
static inline int sys_chdir(const char *path) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(24),"b"(path)); return r;
}
static inline int sys_stat(const char *path, void *st) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(25),"b"(path),"c"(st)); return r;
}
static inline int sys_unlink(const char *path) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(26),"b"(path)); return r;
}
static inline int sys_isatty(int fd) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(29),"b"(fd)); return r;
}
static inline int sys_listdir(const char *path, char *buf, uint32_t sz) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(14),"b"(path),"c"(buf),"d"(sz)); return r;
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

static int do_head(char *argv[], int argc) {
    int n = 10, fi = 1;
    if (argc>1 && argv[1][0]=='-') { n = atoi(argv[1]+1); fi = 2; }
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
    int n = 10, fi = 1;
    if (argc>1 && argv[1][0]=='-') { n = atoi(argv[1]+1); fi = 2; }
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
    while (*p) {
        char *nl=strchr(p,'\n');
        if (nl) *nl=0;
        if (strstr(p,pat)) { fputs(p); putchar('\n'); }
        p = nl ? nl+1 : p+strlen(p);
    }
    return 0;
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
    fputs("    rm <file>         - Delete file\n");
    fputs("    write <f> <data>  - Write text to file\n");
    fputs("    cd [path]         - Change directory\n");
    fputs("    pwd               - Print working directory\n");
    fputs("    stat <file>       - File info\n");
    fputs("    date              - Current date/time\n");
    fputs("    history           - Command history\n");
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
    fputs("  Pipes:  cmd1 | cmd2\n\n");
    return 0;
}

static int do_exec(const char *cmd) {

    char upper[64]; int i=0;
    while(cmd[i]&&i<60){char c=cmd[i];if(c>='a'&&c<='z')c-=32;upper[i++]=c;}
    upper[i]=0;

    if (!strchr(upper, '.')) { strcat(upper, ".ELF"); }

    /* NOTE: this replaces kush itself (real execve, no fork) - a genuine
       fork()+exec()+wait() was attempted here but hit a deep, pre-existing
       kernel bug: paging_clone_dir() aliases (doesn't deep-copy) page
       directory entries below 1GB on the assumption that region is
       kernel-only, but user ELF code actually loads at 0x400000 - well
       within that "shared" range. A forked child's execve() then
       overwrites the same physical pages as its parent's code, so the
       parent resumes executing garbage. A real fix needs page-table-level
       (not just page-directory-level) granularity in paging_clone_dir()/
       paging_free_user() to separate user code from the shared kernel
       heap that lives in the same 4MB region - left for a follow-up
       session rather than shipping something that hangs/crashes. */
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

static int run_pipeline(char *line) {

    if (try_assign(line)) return 0;

    char expanded[CMD_MAX];
    expand_vars(line, expanded, CMD_MAX);
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

    int pipefd[2];
    if (sys_pipe(pipefd) < 0) { fputs("pipe failed\n"); return 1; }

    {
        char *argv[ARG_MAX]; int argc = split(parts[0], argv, ARG_MAX);
        if (!argc) goto cleanup;

        int saved_stdout = sys_dup2(1, 10);
        sys_dup2(pipefd[1], 1);
        close(pipefd[1]);

        const char *cmd = argv[0];
        int ret = 0;
        if (!strcmp(cmd,"ls"))  ret=do_ls(argv,argc);
        else if (!strcmp(cmd,"cat")) ret=do_cat(argv,argc);
        else if (!strcmp(cmd,"echo")) ret=do_echo(argv,argc);
        else if (!strcmp(cmd,"wc")) ret=do_wc(argv,argc);
        else if (!strcmp(cmd,"sort")) ret=do_sort(argv,argc);
        else if (!strcmp(cmd,"uniq")) ret=do_uniq(argv,argc);
        else ret=do_exec(cmd);
        (void)ret;

        sys_dup2(saved_stdout, 1);
        close(saved_stdout);
    }

    {
        char *argv[ARG_MAX]; int argc = split(parts[1], argv, ARG_MAX);
        if (!argc) goto cleanup;

        int saved_stdin = sys_dup2(0, 11);
        sys_dup2(pipefd[0], 0);
        close(pipefd[0]);

        const char *cmd = argv[0];
        int ret = 0;
        if (!strcmp(cmd,"cat")) ret=do_cat(argv,argc);
        else if (!strcmp(cmd,"wc")) ret=do_wc(argv,argc);
        else if (!strcmp(cmd,"sort")) ret=do_sort(argv,argc);
        else if (!strcmp(cmd,"uniq")) ret=do_uniq(argv,argc);
        else ret=do_exec(cmd);
        (void)ret;

        sys_dup2(saved_stdin, 0);
        close(saved_stdin);
    }

cleanup:
    return 0;
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

static int run_script(const char *filename) {
    int fd = open(filename);
    if (fd < 0) { printf("kush: %s: not found\n", filename); return 1; }
    char buf[4096]; int n = fread(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;

    char *p = buf; int ret = 0;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        while (*p == ' ' || *p == '\t') p++;
        if (*p && *p != '#') {
            char line[256]; strncpy(line, p, 255);
            ret = run_pipeline(line);
        }
        p = nl ? nl+1 : p+strlen(p);
    }
    return ret;
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
        hist_add(line);
        last_exit = run_pipeline(line);
    }
}