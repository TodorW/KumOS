#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "disk-io.h"

#define SECTOR_SIZE 512
#define DISK_SIZE (1024 * 1024 * 128)
#define MAX_CACHE_SIZE 1024
#define MAX_RETRIES 3
#define MAX_PARTITIONS 4
#define MAX_DISK_QUOTA (DISK_SIZE / 2)

typedef struct {
    unsigned int sector;
    char data[SECTOR_SIZE];
} CacheEntry;

typedef struct {
    unsigned int read_count;
    unsigned int write_count;
    unsigned int error_count;
} DiskStats;

static FILE *disk_file = NULL;
static CacheEntry cache[MAX_CACHE_SIZE];
static int cache_count = 0;
static DiskStats stats = {0, 0, 0};

typedef void (*AsyncCallback)(int result, void *data);
typedef struct {
    unsigned int sector;
    void *buffer;
    AsyncCallback callback;
    int is_write;
    time_t request_time;
} AsyncRequest;

typedef struct {
    unsigned int start_sector;
    unsigned int end_sector;
    char label[32];
} Partition;

static Partition partitions[MAX_PARTITIONS];
static int partition_count = 0;
static unsigned int disk_usage = 0;
static const char encryption_key = 0xAB;

int disk_initialize(const char *disk_path) {
    disk_file = fopen(disk_path, "r+b");
    if (!disk_file) {
        disk_file = fopen(disk_path, "w+b");
        if (!disk_file) {
            perror("Failed to open or create disk file");
            return -1;
        }
    }
    printf("Disk initialized successfully.\n");
    return 0;
}

void disk_shutdown() {
    if (disk_file) {
        fclose(disk_file);
        disk_file = NULL;
    }
}

int disk_retry_operation(int (*operation)(unsigned int, void*), unsigned int sector, void *buffer) {
    int retries = 0;
    while (retries < MAX_RETRIES) {
        if (operation(sector, buffer) == 0) return 0;
        retries++;
    }
    stats.error_count++;
    return -1;
}

int cache_lookup(unsigned int sector, void *buffer) {
    for (int i = 0; i < cache_count; i++) {
        if (cache[i].sector == sector) {
            memcpy(buffer, cache[i].data, SECTOR_SIZE);
            return 1;
        }
    }
    return 0;
}

void cache_insert(unsigned int sector, const void *buffer) {
    if (cache_count >= MAX_CACHE_SIZE) cache_count = 0;
    cache[cache_count].sector = sector;
    memcpy(cache[cache_count].data, buffer, SECTOR_SIZE);
    cache_count++;
}

void encrypt_data(void *buffer) {
    for (int i = 0; i < SECTOR_SIZE; i++) {
        ((char *)buffer)[i] ^= encryption_key;
    }
}

int disk_read_sector(unsigned int sector, void *buffer) {
    if (cache_lookup(sector, buffer)) {
        stats.read_count++;
        return 0;
    }
    fseek(disk_file, sector * SECTOR_SIZE, SEEK_SET);
    if (fread(buffer, 1, SECTOR_SIZE, disk_file) != SECTOR_SIZE) {
        fprintf(stderr, "Error reading sector %u\n", sector);
        return -1;
    }
    encrypt_data(buffer);
    cache_insert(sector, buffer);
    stats.read_count++;
    return 0;
}

int disk_write_sector(unsigned int sector, const void *buffer) {
    if (disk_usage + SECTOR_SIZE > MAX_DISK_QUOTA) {
        fprintf(stderr, "Disk quota exceeded\n");
        return -1;
    }
    void *encrypted_buffer = malloc(SECTOR_SIZE);
    memcpy(encrypted_buffer, buffer, SECTOR_SIZE);
    encrypt_data(encrypted_buffer);
    fseek(disk_file, sector * SECTOR_SIZE, SEEK_SET);
    if (fwrite(encrypted_buffer, 1, SECTOR_SIZE, disk_file) != SECTOR_SIZE) {
        fprintf(stderr, "Error writing to sector %u\n", sector);
        free(encrypted_buffer);
        return -1;
    }
    free(encrypted_buffer);
    cache_insert(sector, buffer);
    stats.write_count++;
    disk_usage += SECTOR_SIZE;
    return 0;
}

int add_partition(unsigned int start_sector, unsigned int end_sector, const char *label) {
    if (partition_count >= MAX_PARTITIONS) {
        fprintf(stderr, "Max partitions reached\n");
        return -1;
    }
    partitions[partition_count].start_sector = start_sector;
    partitions[partition_count].end_sector = end_sector;
    strncpy(partitions[partition_count].label, label, sizeof(partitions[partition_count].label) - 1);
    partitions[partition_count].label[sizeof(partitions[partition_count].label) - 1] = '\0';
    partition_count++;
    return 0;
}

void log_operation(const char *operation, unsigned int sector) {
    time_t current_time = time(NULL);
    char *timestamp = ctime(&current_time);
    timestamp[strlen(timestamp) - 1] = '\0'; // Remove newline character
    printf("[%s] [LOG] %s sector %u\n", timestamp, operation, sector);
}

void print_stats() {
    printf("Disk Read Count: %u\n", stats.read_count);
    printf("Disk Write Count: %u\n", stats.write_count);
    printf("Disk Error Count: %u\n", stats.error_count);
}

void *async_io_thread(void *arg) {
    AsyncRequest *req = (AsyncRequest *)arg;
    int result;
    if (req->is_write) {
        result = disk_write_sector(req->sector, req->buffer);
    } else {
        result = disk_read_sector(req->sector, req->buffer);
    }
    req->callback(result, req->buffer);
    free(req);
    return NULL;
}

int async_io(unsigned int sector, void *buffer, int is_write, AsyncCallback callback) {
    AsyncRequest *req = malloc(sizeof(AsyncRequest));
    req->sector = sector;
    req->buffer = buffer;
    req->callback = callback;
    req->is_write = is_write;
    req->request_time = time(NULL);
    pthread_t thread;
    pthread_create(&thread, NULL, async_io_thread, req);
    pthread_detach(thread);
    return 0;
}

void io_callback(int result, void *data) {
    if (result == 0) {
        printf("Async I/O operation successful\n");
    } else {
        printf("Async I/O operation failed\n");
    }
}

int main() {
    disk_initialize("disk.img");

    char buffer[SECTOR_SIZE] = "Hello, KumOS!";
    async_io(0, buffer, 1, io_callback);
    async_io(0, buffer, 0, io_callback);

    add_partition(0, 1024, "System");
    add_partition(1024, 2048, "Data");

    log_operation("Read", 0);
    log_operation("Write", 1024);

    print_stats();
    disk_shutdown();
    return 0;
}
