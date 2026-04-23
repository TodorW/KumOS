#include "kumos_libc.h"

static int do_wc(void) {
    char buf[4096]; int n;
    uint32_t lines=0, words=0, bytes=0;
    int in_word=0;
    while((n=read(buf,sizeof(buf)-1))>0) {
        buf[n]=0; bytes+=(uint32_t)n;
        for(int i=0;i<n;i++) {
            if(buf[i]=='\n') lines++;
            if(buf[i]==' '||buf[i]=='\t'||buf[i]=='\n') in_word=0;
            else if(!in_word) { in_word=1; words++; }
        }
    }
    printf("%6u %6u %6u\n", lines, words, bytes);
    return 0;
}

static int do_sort(void) {
    char *lines[512]; int nlines=0;
    static char buf[32768]; int total=0; int n;
    while((n=read(buf+total,(uint32_t)(sizeof(buf)-total-1)))>0) total+=n;
    buf[total]=0;
    char *p=buf;
    while(*p&&nlines<512) {
        char *nl=strchr(p,'\n');
        if(nl)*nl=0;
        lines[nlines++]=p;
        p=nl?nl+1:p+strlen(p);
    }
    qsort(lines,(size_t)nlines,sizeof(char*),(int(*)(const void*,const void*))strcmp);
    for(int i=0;i<nlines;i++) { fputs(lines[i]); putchar('\n'); }
    return 0;
}

static int do_uniq(void) {
    char buf[4096]; int n; int total=0;
    static char all[32768];
    while((n=read(all+total,(uint32_t)(sizeof(all)-total-1)))>0) total+=n;
    all[total]=0;
    char prev[512]; prev[0]=0;
    char *p=all;
    while(*p) {
        char *nl=strchr(p,'\n'); if(nl)*nl=0;
        if(strcmp(p,prev)!=0) {
            fputs(p); putchar('\n');
            strncpy(prev,p,511);
        }
        p=nl?nl+1:p+strlen(p);
    }
    (void)buf;
    return 0;
}

int main(void) {
    char args[64]; int n=read(args,63);
    if(n>0){args[n]=0;while(n>0&&(args[n-1]=='\n'||args[n-1]==' '))args[--n]=0;}

    if(!strcmp(args,"wc"))   return do_wc();
    if(!strcmp(args,"sort")) return do_sort();
    if(!strcmp(args,"uniq")) return do_uniq();

    puts("Usage: wc | sort | uniq");
    puts("  These tools read from stdin.");
    puts("  Use pipes: ls | sort");
    return 1;
}
