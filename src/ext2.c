#include "ext2.h"
#include "ata.h"
#include "kstring.h"
#include "vfs.h"
#include <stdint.h>

static ext2_super_t sb;
static ext2_bgd_t   bgd;
static uint32_t     block_size = 1024;
static uint32_t     lba_base   = 0;
static int          mounted    = 0;

static void read_block(uint32_t blk, void *buf) {
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = lba_base + blk * sectors_per_block;
    uint8_t *p = (uint8_t *)buf;
    for (uint32_t i = 0; i < sectors_per_block; i++) {
        ata_read(0, lba + i, 1, p);
        p += 512;
    }
}

static void read_inode(uint32_t ino, ext2_inode_t *out) {
    uint32_t inodes_per_block = block_size / sb.s_inode_size;
    uint32_t group  = (ino - 1) / sb.s_inodes_per_group;
    uint32_t idx    = (ino - 1) % sb.s_inodes_per_group;
    uint32_t blk    = bgd.bg_inode_table + idx / inodes_per_block;
    uint32_t offset = (idx % inodes_per_block) * sb.s_inode_size;
    uint8_t  buf[4096];
    (void)group;
    read_block(blk, buf);
    kmemcpy(out, buf + offset, sizeof(ext2_inode_t));
}

static uint32_t get_block(ext2_inode_t *ino, uint32_t n) {
    if (n < 12) return ino->i_block[n];
    uint32_t indirect[1024];
    if (n < 12 + block_size/4) {
        read_block(ino->i_block[12], indirect);
        return indirect[n - 12];
    }
    return 0;
}

static uint32_t lookup_dir(uint32_t dir_ino, const char *name) {
    ext2_inode_t inode;
    read_inode(dir_ino, &inode);
    uint32_t size = inode.i_size;
    uint32_t read = 0;
    uint8_t  blk[4096];
    uint32_t nlen = (uint32_t)kstrlen(name);

    for (uint32_t b = 0; read < size; b++) {
        uint32_t bno = get_block(&inode, b);
        if (!bno) break;
        read_block(bno, blk);
        uint32_t off = 0;
        uint32_t bsz = size - read > block_size ? block_size : size - read;
        while (off < bsz) {
            ext2_dirent_t *d = (ext2_dirent_t *)(blk + off);
            if (!d->rec_len) break;
            if (d->inode && d->name_len == nlen &&
                kstrncmp(d->name, name, nlen) == 0)
                return d->inode;
            off += d->rec_len;
        }
        read += block_size;
    }
    return 0;
}

static uint32_t path_to_ino(const char *path) {
    if (!*path || (path[0]=='/'&&!path[1])) return EXT2_ROOT_INO;
    const char *p = path;
    if (*p=='/') p++;
    uint32_t ino = EXT2_ROOT_INO;
    char part[256];
    while (*p) {
        const char *slash = p;
        while (*slash && *slash!='/') slash++;
        uint32_t len = (uint32_t)(slash - p);
        kmemcpy(part, p, len); part[len]=0;
        ino = lookup_dir(ino, part);
        if (!ino) return 0;
        p = *slash ? slash+1 : slash;
    }
    return ino;
}

int ext2_init(uint32_t lba_start) {
    lba_base = lba_start;
    uint8_t tmp[1024];
    ata_read(0, lba_start+2, 1, tmp);
    ata_read(0, lba_start+3, 1, tmp+512);
    kmemcpy(&sb, tmp, sizeof(ext2_super_t));
    if (sb.s_magic != EXT2_MAGIC) return -1;
    block_size = 1024u << sb.s_log_block_size;
    uint8_t bgd_buf[512];
    uint32_t bgd_lba = lba_start + (block_size==1024?4:block_size/512);
    ata_read(0, bgd_lba, 1, bgd_buf);
    kmemcpy(&bgd, bgd_buf, sizeof(ext2_bgd_t));
    mounted = 1;
    return 0;
}

int ext2_mounted(void) { return mounted; }

int ext2_read_file(const char *path, void *buf, uint32_t sz) {
    if (!mounted) return -1;
    uint32_t ino = path_to_ino(path);
    if (!ino) return -1;
    ext2_inode_t inode;
    read_inode(ino, &inode);
    uint32_t total = inode.i_size < sz ? inode.i_size : sz;
    uint8_t blk[4096]; uint32_t done=0;
    for (uint32_t b=0; done<total; b++) {
        uint32_t bno=get_block(&inode,b); if(!bno) break;
        read_block(bno, blk);
        uint32_t chunk=total-done>block_size?block_size:total-done;
        kmemcpy((uint8_t*)buf+done, blk, chunk);
        done+=chunk;
    }
    return (int)done;
}

int ext2_list_dir(const char *path, char *buf, uint32_t sz) {
    if (!mounted) return -1;
    uint32_t ino = path_to_ino(path);
    if (!ino) return -1;
    ext2_inode_t inode;
    read_inode(ino, &inode);
    uint8_t blk[4096]; uint32_t pos=0, read=0;
    for (uint32_t b=0; read<inode.i_size; b++) {
        uint32_t bno=get_block(&inode,b); if(!bno) break;
        read_block(bno, blk);
        uint32_t off=0;
        while(off<block_size) {
            ext2_dirent_t *d=(ext2_dirent_t*)(blk+off);
            if(!d->rec_len) break;
            if(d->inode && d->name_len &&
               !(d->name_len==1&&d->name[0]=='.') &&
               !(d->name_len==2&&d->name[0]=='.'&&d->name[1]=='.')) {
                if(pos+d->name_len+2<sz) {
                    kmemcpy(buf+pos,d->name,d->name_len);
                    pos+=d->name_len;
                    buf[pos++]='\n';
                }
            }
            off+=d->rec_len;
        }
        read+=block_size;
    }
    buf[pos]=0;
    return (int)pos;
}

int ext2_stat(const char *path, uint32_t *size_out) {
    if (!mounted) return -1;
    uint32_t ino = path_to_ino(path);
    if (!ino) return -1;
    ext2_inode_t inode; read_inode(ino, &inode);
    if (size_out) *size_out = inode.i_size;
    return 0;
}

static int e2_open(const char *path, int flags) {
    (void)flags;
    uint32_t ino = path_to_ino(path);
    return ino ? (int)ino : -1;
}
static int e2_close(int d)  { (void)d; return 0; }
static int e2_read(int d, void *buf, uint32_t len) {
    ext2_inode_t inode; read_inode((uint32_t)d, &inode);
    uint32_t total=inode.i_size<len?inode.i_size:len;
    uint8_t blk[4096]; uint32_t done=0;
    for(uint32_t b=0;done<total;b++){
        uint32_t bno=get_block(&inode,b); if(!bno)break;
        read_block(bno,blk);
        uint32_t chunk=total-done>block_size?block_size:total-done;
        kmemcpy((uint8_t*)buf+done,blk,chunk); done+=chunk;
    }
    return (int)done;
}
static int e2_write(int d, const void *buf, uint32_t len) {
    (void)d;(void)buf;(void)len; return -1;
}
static int e2_stat(const char *path, vfs_stat_t *st) {
    uint32_t size=0;
    if(ext2_stat(path,&size)<0) return -1;
    st->size=size; st->type=VFS_FILE;
    kstrcpy(st->name,path);
    return 0;
}
static int e2_readdir(const char *path, char *buf, uint32_t sz) {
    return ext2_list_dir(path,buf,sz);
}

vfs_ops_t ext2_vfs_ops = {
    e2_open, e2_close, e2_read, e2_write, e2_stat, e2_readdir, 0, 0
};
