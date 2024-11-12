#include <stdint.h>

#define SECTOR_SIZE 512

// BIOS Disk Services
#define BIOS_DISK_READ  0x02
#define BIOS_DISK_WRITE 0x03
#define BIOS_RESET_DISK 0x00

// Disk access via BIOS interrupt 0x13
static inline uint8_t bios_disk_io(uint8_t function, uint8_t drive, uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t count, uint16_t segment, uint16_t offset) {
    uint8_t status;
    asm volatile (
        "int $0x13"
        : "=a"(status)  // Output: status in AL
        : "a"(function << 8), "d"(drive), "c"((cylinder << 8) | sector), "b"(head), "S"(segment), "x"(offset)
        : "cc", "memory"
    );
    return status;
}

// Initialize disk (typically resets the drive)
int disk_initialize(uint8_t drive) {
    return bios_disk_io(BIOS_RESET_DISK, drive, 0, 0, 0, 0, 0, 0) == 0;
}

// Read a single sector from disk
int disk_read_sector(uint8_t drive, uint32_t lba, uint8_t* buffer) {
    uint16_t cylinder = (lba / (18 * 2)) & 0xFF;   // Calculate CHS from LBA
    uint8_t head = (lba / 18) % 2;
    uint8_t sector = (lba % 18) + 1;
    
    uint16_t segment = ((uint32_t) buffer) >> 4;   // Segment address
    uint16_t offset = ((uint32_t) buffer) & 0x0F;  // Offset address

    return bios_disk_io(BIOS_DISK_READ, drive, cylinder, head, sector, 1, segment, offset) == 0;
}

// Write a single sector to disk
int disk_write_sector(uint8_t drive, uint32_t lba, const uint8_t* buffer) {
    uint16_t cylinder = (lba / (18 * 2)) & 0xFF;   // Calculate CHS from LBA
    uint8_t head = (lba / 18) % 2;
    uint8_t sector = (lba % 18) + 1;

    uint16_t segment = ((uint32_t) buffer) >> 4;   // Segment address
    uint16_t offset = ((uint32_t) buffer) & 0x0F;  // Offset address

    return bios_disk_io(BIOS_DISK_WRITE, drive, cylinder, head, sector, 1, segment, offset) == 0;
}

// Higher-level function to read multiple sectors
int disk_read(uint8_t drive, uint32_t lba, uint8_t* buffer, uint32_t sectors) {
    for (uint32_t i = 0; i < sectors; i++) {
        if (!disk_read_sector(drive, lba + i, buffer + (i * SECTOR_SIZE))) {
            return 0;  // Return failure if any read fails
        }
    }
    return 1;  // Success
}

// Higher-level function to write multiple sectors
int disk_write(uint8_t drive, uint32_t lba, const uint8_t* buffer, uint32_t sectors) {
    for (uint32_t i = 0; i < sectors; i++) {
        if (!disk_write_sector(drive, lba + i, buffer + (i * SECTOR_SIZE))) {
            return 0;  // Return failure if any write fails
        }
    }
    return 1;  // Success
}
