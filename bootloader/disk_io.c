#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disk-io.h"  // Assuming there is a header file to declare functions and data structures

#define SECTOR_SIZE 512  // Standard sector size in bytes
#define DISK_SIZE (1024 * 1024 * 128) // Example disk size (128MB)

static FILE *disk_file = NULL;  // File pointer to simulate a disk

// Initialize disk
int disk_initialize(const char *disk_path) {
    // Open the file to simulate the disk
    disk_file = fopen(disk_path, "r+b");
    if (!disk_file) {
        // Create a new file if it doesn't exist
        disk_file = fopen(disk_path, "w+b");
        if (!disk_file) {
            perror("Failed to open or create disk file");
            return -1;
        }
        // Optionally, you could pre-allocate disk space here if required.
    }
    printf("Disk initialized successfully.\n");
    return 0;
}

// Shutdown disk
void disk_shutdown() {
    if (disk_file) {
        fclose(disk_file);
        disk_file = NULL;
        printf("Disk shut down successfully.\n");
    }
}

// Read sector from disk
int disk_read_sector(unsigned int sector_number, void *buffer) {
    if (!disk_file) {
        fprintf(stderr, "Disk not initialized.\n");
        return -1;
    }
    fseek(disk_file, sector_number * SECTOR_SIZE, SEEK_SET);
    size_t read_size = fread(buffer, 1, SECTOR_SIZE, disk_file);
    if (read_size != SECTOR_SIZE) {
        fprintf(stderr, "Error reading sector %u\n", sector_number);
        return -1;
    }
    return 0;
}

// Write sector to disk
int disk_write_sector(unsigned int sector_number, const void *buffer) {
    if (!disk_file) {
        fprintf(stderr, "Disk not initialized.\n");
        return -1;
    }
    fseek(disk_file, sector_number * SECTOR_SIZE, SEEK_SET);
    size_t write_size = fwrite(buffer, 1, SECTOR_SIZE, disk_file);
    if (write_size != SECTOR_SIZE) {
        fprintf(stderr, "Error writing to sector %u\n", sector_number);
        return -1;
    }
    fflush(disk_file); // Ensure data is written to the disk file
    return 0;
}

// Utility function to get disk size in sectors
unsigned int disk_get_sector_count() {
    if (!disk_file) {
        fprintf(stderr, "Disk not initialized.\n");
        return 0;
    }
    fseek(disk_file, 0, SEEK_END);
    long file_size = ftell(disk_file);
    return (unsigned int)(file_size / SECTOR_SIZE);
}

