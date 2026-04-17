
#include "fat12.h"
#include "ata.h"
#include "vga.h"
#include "kstring.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} bpb_t;

typedef struct __attribute__((packed)) {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t start_cluster;
    uint32_t file_size;
} dirent_t;

#define ATTR_READONLY  0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME    0x08
#define ATTR_DIR       0x10
#define ATTR_ARCHIVE   0x20

#define DIRENT_FREE    0xE5
#define DIRENT_END     0x00

static int      g_drive        = -1;
static int      g_mounted      = 0;
static bpb_t    g_bpb;
static uint32_t g_fat_start;
static uint32_t g_root_start;
static uint32_t g_data_start;
static uint32_t g_root_sectors;
static uint32_t g_total_clusters;

#define FAT_BUF_SIZE  (9 * 512)
static uint8_t g_fat[FAT_BUF_SIZE];
static int     g_fat_dirty = 0;

static uint8_t g_sector[512];

static uint32_t cluster_to_lba(uint16_t cluster) {
    return g_data_start + (cluster - 2) * g_bpb.sectors_per_cluster;
}

static uint16_t fat_get(uint16_t n) {
    uint32_t idx = n + (n / 2);
    uint16_t val = g_fat[idx] | ((uint16_t)g_fat[idx+1] << 8);
    return (n & 1) ? (val >> 4) : (val & 0x0FFF);
}

static void fat_set(uint16_t n, uint16_t val) {
    uint32_t idx = n + (n / 2);
    if (n & 1) {
        g_fat[idx]   = (g_fat[idx] & 0x0F) | ((val & 0x0F) << 4);
        g_fat[idx+1] = (val >> 4) & 0xFF;
    } else {
        g_fat[idx]   = val & 0xFF;
        g_fat[idx+1] = (g_fat[idx+1] & 0xF0) | ((val >> 8) & 0x0F);
    }
    g_fat_dirty = 1;
}

static uint16_t fat_alloc(void) {
    for (uint16_t i = 2; i < g_total_clusters + 2; i++) {
        if (fat_get(i) == 0x000) {
            fat_set(i, 0xFFF);
            return i;
        }
    }
    return 0;
}

static int fat_flush(void) {
    if (!g_fat_dirty) return 0;
    int sects = g_bpb.sectors_per_fat;
    for (int copy = 0; copy < g_bpb.fat_count; copy++) {
        for (int s = 0; s < sects; s++) {
            if (ata_write(g_drive, g_fat_start + copy*sects + s,
                          1, g_fat + s*512) < 0) return -1;
        }
    }
    g_fat_dirty = 0;
    return 0;
}

static void name_to_83(const char *name, char out[11]) {
    kmemset(out, ' ', 11);
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[j++] = c;
    }
    if (name[i] == '.') {
        i++; j = 8;
        while (name[i] && j < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[j++] = c;
        }
    }
}

static void name_from_83(const char raw[11], char *out) {
    int i = 0, j = 0;
    while (i < 8 && raw[i] != ' ') out[j++] = raw[i++];
    if (raw[8] != ' ') {
        out[j++] = '.';
        for (i = 8; i < 11 && raw[i] != ' '; i++) out[j++] = raw[i];
    }
    out[j] = 0;
}

int fat12_mount(int drive) {
    g_mounted = 0;
    g_drive   = drive;

    if (ata_read(drive, 0, 1, &g_bpb) < 0) return -1;

    if (kstrncmp(g_bpb.fs_type, "FAT12", 5) != 0 &&
        kstrncmp(g_bpb.fs_type, "FAT   ", 5) != 0) {

        if (g_bpb.bytes_per_sector != 512) return -1;
        if (g_bpb.fat_count < 1 || g_bpb.fat_count > 2) return -1;
        if (g_bpb.root_entry_count == 0) return -1;
    }

    g_fat_start    = g_bpb.reserved_sectors;
    g_root_start   = g_fat_start + (uint32_t)g_bpb.fat_count * g_bpb.sectors_per_fat;
    g_root_sectors = (g_bpb.root_entry_count * 32 + 511) / 512;
    g_data_start   = g_root_start + g_root_sectors;

    uint32_t total = g_bpb.total_sectors_16 ? g_bpb.total_sectors_16
                                             : g_bpb.total_sectors_32;
    uint32_t data_sectors = total - g_data_start;
    g_total_clusters = data_sectors / g_bpb.sectors_per_cluster;

    uint32_t fat_bytes = (uint32_t)g_bpb.sectors_per_fat * 512;
    if (fat_bytes > FAT_BUF_SIZE) fat_bytes = FAT_BUF_SIZE;
    for (uint32_t s = 0; s < g_bpb.sectors_per_fat; s++) {
        if (ata_read(drive, g_fat_start + s, 1, g_fat + s*512) < 0) return -1;
    }

    g_mounted  = 1;
    g_fat_dirty = 0;
    return 0;
}

int fat12_mounted(void) { return g_mounted; }

int fat12_list(fat12_entry_t *entries, int max) {
    if (!g_mounted) return -1;
    int count = 0;
    dirent_t de;

    for (uint32_t s = 0; s < g_root_sectors && count < max; s++) {
        if (ata_read(g_drive, g_root_start + s, 1, g_sector) < 0) return count;
        dirent_t *dir = (dirent_t *)g_sector;
        for (int i = 0; i < 512/32 && count < max; i++, dir++) {
            uint8_t first = (uint8_t)dir->name[0];
            if (first == DIRENT_END) goto done;
            if (first == DIRENT_FREE) continue;
            if (dir->attr & (ATTR_VOLUME | ATTR_SYSTEM)) continue;
            if (dir->attr & ATTR_HIDDEN) continue;
            (void)de;
            name_from_83(dir->name, entries[count].name);
            entries[count].size          = dir->file_size;
            entries[count].start_cluster = dir->start_cluster;
            entries[count].attr          = dir->attr;
            entries[count].is_dir        = (dir->attr & ATTR_DIR) ? 1 : 0;
            count++;
        }
    }
done:
    return count;
}

typedef struct { uint32_t sector; int index; dirent_t de; int found; } dirfind_t;

static dirfind_t fat12_find(const char *name) {
    dirfind_t r; r.found = 0;
    char want[11]; name_to_83(name, want);
    for (uint32_t s = 0; s < g_root_sectors; s++) {
        if (ata_read(g_drive, g_root_start + s, 1, g_sector) < 0) return r;
        dirent_t *dir = (dirent_t *)g_sector;
        for (int i = 0; i < 512/32; i++, dir++) {
            uint8_t first = (uint8_t)dir->name[0];
            if (first == DIRENT_END) return r;
            if (first == DIRENT_FREE) continue;
            if (kstrncmp(dir->name, want, 11) == 0) {
                r.found  = 1;
                r.sector = g_root_start + s;
                r.index  = i;
                kmemcpy(&r.de, dir, sizeof(dirent_t));
                return r;
            }
        }
    }
    return r;
}

int fat12_read(const char *name, void *buf, uint32_t bufsz) {
    if (!g_mounted) return -1;
    dirfind_t f = fat12_find(name);
    if (!f.found) return -1;

    uint32_t remaining = f.de.file_size;
    if (remaining > bufsz) remaining = bufsz;
    uint32_t read_bytes = 0;
    uint16_t cluster = f.de.start_cluster;
    uint8_t *out = (uint8_t *)buf;

    while (cluster >= 0x002 && cluster <= 0xFEF && remaining > 0) {
        uint32_t lba = cluster_to_lba(cluster);
        for (int s = 0; s < g_bpb.sectors_per_cluster && remaining > 0; s++) {
            if (ata_read(g_drive, lba + s, 1, g_sector) < 0) return read_bytes;
            uint32_t take = remaining > 512 ? 512 : remaining;
            kmemcpy(out, g_sector, take);
            out        += take;
            read_bytes += take;
            remaining  -= take;
        }
        cluster = fat_get(cluster);
    }
    return (int)read_bytes;
}

int fat12_write(const char *name, const void *buf, uint32_t size) {
    if (!g_mounted) return -1;

    fat12_delete(name);

    uint16_t first_cluster = 0;
    uint16_t prev_cluster  = 0;
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t remaining = size;

    if (size == 0) {
        first_cluster = 0;
    } else {
        while (remaining > 0) {
            uint16_t c = fat_alloc();
            if (!c) return -1;
            if (!first_cluster) first_cluster = c;
            if (prev_cluster)   fat_set(prev_cluster, c);
            prev_cluster = c;

            uint32_t lba = cluster_to_lba(c);
            for (int s = 0; s < g_bpb.sectors_per_cluster && remaining > 0; s++) {
                uint32_t take = remaining > 512 ? 512 : remaining;
                kmemset(g_sector, 0, 512);
                kmemcpy(g_sector, src, take);
                if (ata_write(g_drive, lba + s, 1, g_sector) < 0) return -1;
                src       += take;
                remaining -= take;
            }
        }
        fat_set(prev_cluster, 0xFFF);
    }

    if (fat_flush() < 0) return -1;

    char fn83[11]; name_to_83(name, fn83);
    for (uint32_t s = 0; s < g_root_sectors; s++) {
        if (ata_read(g_drive, g_root_start + s, 1, g_sector) < 0) return -1;
        dirent_t *dir = (dirent_t *)g_sector;
        for (int i = 0; i < 512/32; i++, dir++) {
            uint8_t first = (uint8_t)dir->name[0];
            if (first == DIRENT_FREE || first == DIRENT_END) {

                kmemset(dir, 0, sizeof(dirent_t));
                kmemcpy(dir->name, fn83, 11);
                dir->attr          = ATTR_ARCHIVE;
                dir->start_cluster = first_cluster;
                dir->file_size     = size;
                dir->date          = 0x4A21;
                dir->time          = 0x0000;

                if (first == DIRENT_END && i + 1 < 512/32)
                    (dir+1)->name[0] = DIRENT_END;
                return ata_write(g_drive, g_root_start + s, 1, g_sector);
            }
        }
    }
    return -1;
}

int fat12_delete(const char *name) {
    if (!g_mounted) return -1;
    dirfind_t f = fat12_find(name);
    if (!f.found) return -1;

    uint16_t cluster = f.de.start_cluster;
    while (cluster >= 0x002 && cluster <= 0xFEF) {
        uint16_t next = fat_get(cluster);
        fat_set(cluster, 0x000);
        cluster = next;
    }
    fat_flush();

    if (ata_read(g_drive, f.sector, 1, g_sector) < 0) return -1;
    dirent_t *dir = (dirent_t *)g_sector;
    dir[f.index].name[0] = DIRENT_FREE;
    return ata_write(g_drive, f.sector, 1, g_sector);
}

int fat12_format(int drive, const char *label) {

    uint8_t boot[512];
    kmemset(boot, 0, 512);

    bpb_t *b = (bpb_t *)boot;
    b->jmp[0] = 0xEB; b->jmp[1] = 0x3C; b->jmp[2] = 0x90;
    kmemcpy(b->oem, "KUMOS1.0", 8);
    b->bytes_per_sector    = 512;
    b->sectors_per_cluster = 1;
    b->reserved_sectors    = 1;
    b->fat_count           = 2;
    b->root_entry_count    = 224;
    b->total_sectors_16    = 2880;
    b->media_type          = 0xF0;
    b->sectors_per_fat     = 9;
    b->sectors_per_track   = 18;
    b->head_count          = 2;
    b->boot_sig            = 0x29;
    b->volume_id           = 0x4B554D4F;
    kmemset(b->volume_label, ' ', 11);
    if (label) {
        int l = kstrlen(label);
        if (l > 11) l = 11;
        for (int i = 0; i < l; i++) {
            char c = label[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            b->volume_label[i] = c;
        }
    }
    kmemcpy(b->fs_type, "FAT12   ", 8);
    boot[510] = 0x55; boot[511] = 0xAA;

    if (ata_write(drive, 0, 1, boot) < 0) return -1;

    uint8_t fat[512];
    for (int copy = 0; copy < 2; copy++) {
        for (int s = 0; s < 9; s++) {
            kmemset(fat, 0, 512);
            if (s == 0) {
                fat[0] = 0xF0; fat[1] = 0xFF; fat[2] = 0xFF;
            }
            if (ata_write(drive, 1 + copy*9 + s, 1, fat) < 0) return -1;
        }
    }

    uint8_t root[512];
    kmemset(root, 0, 512);
    for (int s = 0; s < 14; s++) {
        if (ata_write(drive, 19 + s, 1, root) < 0) return -1;
    }

    return fat12_mount(drive);
}

void fat12_info(void) {
    if (!g_mounted) { vga_puts("  No FAT12 volume mounted.\n"); return; }
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n  === FAT12 Volume ===\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    char label[12]; kmemcpy(label, g_bpb.volume_label, 11); label[11]=0;
    vga_puts("  Label:    "); vga_puts(label); vga_putchar('\n');
    vga_puts("  FAT start LBA:  "); vga_put_dec(g_fat_start);   vga_putchar('\n');
    vga_puts("  Root start LBA: "); vga_put_dec(g_root_start);  vga_putchar('\n');
    vga_puts("  Data start LBA: "); vga_put_dec(g_data_start);  vga_putchar('\n');
    vga_puts("  Total clusters: "); vga_put_dec(g_total_clusters); vga_putchar('\n');

    uint16_t free_cl = 0;
    for (uint16_t i = 2; i < g_total_clusters + 2; i++)
        if (fat_get(i) == 0x000) free_cl++;
    vga_puts("  Free clusters:  "); vga_put_dec(free_cl);
    vga_puts("  ("); vga_put_dec(free_cl * 512 / 1024); vga_puts(" KB free)\n\n");
}