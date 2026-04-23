
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
    fputs("    cp <src> <dst>    - Copy file\n");
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

    int r = sys_exec(upper);
    if (r < 0) { printf("kush: %s: not found\n", cmd); return 127; }
    return r;
}

static int run_script(const char *filename);
static int run_pipeline(char *line) {

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
        const char *cmd = argv[0];
        if (!strcmp(cmd,"exit")){exit(argc>1?atoi(argv[1]):0);}
        if (!strcmp(cmd,"source")||!strcmp(cmd,".")) {
            if(argc>1) return run_script(argv[1]);
            puts("source: need filename"); return 1;
        }
        if (!strcmp(cmd,"help"))    return do_help(argv,argc);
        if (!strcmp(cmd,"ls"))      return do_ls(argv,argc);
        if (!strcmp(cmd,"cat"))     return do_cat(argv,argc);
        if (!strcmp(cmd,"cp"))      return do_cp(argv,argc);
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
    fputs("  ██╗  ██╗██╗   ██╗███████╗██╗  ██╗\n");
    fputs("  ██║ ██╔╝██║   ██║██╔════╝██║  ██║\n");
    fputs("  █████╔╝ ██║   ██║███████╗███████║\n");
    fputs("  ██╔═██╗ ██║   ██║╚════██║██╔══██║\n");
    fputs("  ██║  ██╗╚██████╔╝███████║██║  ██║\n");
    fputs("  ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝\n");
    printf("\n  kush v%s — KumOS Shell  (type 'help')\n\n", KUSH_VER);
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