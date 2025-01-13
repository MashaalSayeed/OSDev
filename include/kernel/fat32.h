#pragma once

#include <stdint.h>
#include "kernel/vfs.h"

#define CLUSTER_SIZE 4096

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN 0x02
#define FAT32_ATTR_SYSTEM 0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE 0x20
#define FAT32_ATTR_DEVICE 0x40
#define FAT32_ATTR_RESERVED 0x80

#define FAT32_ATTR_LONG_NAME 0x0F
#define FAT32_LONG_NAME_MASK 0x40

#define FAT32_CLUSTER_FREE 0x00000000
#define FAT32_CLUSTER_RESERVED 0x0FFFFFF0
#define FAT32_CLUSTER_BAD 0x0FFFFFF7
#define FAT32_CLUSTER_LAST 0x0FFFFFF8

#define FAT32_LAST_ENTRY 0x00
#define FAT32_DELETED_ENTRY 0xE5

#define FAT32_SIGNATURE 0xAA55

typedef struct {
    uint8_t fat_count;
    uint32_t fat_size;
    uint32_t root_cluster;
    uint32_t root_sector;
    uint32_t root_entries;
    uint16_t reserved_sectors;
    uint8_t sectors_per_cluster;
    uint32_t data_sector;
    uint32_t total_sectors;
    uint32_t data_clusters;
    uint32_t cluster_size;
    uint16_t bytes_per_sector;
    block_device_t *device;
} fat32_superblock_t;

typedef struct {
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_large;
    uint32_t sectors_per_fat_large;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    char boot_code[420];
    uint16_t boot_signature2;
} __attribute__((packed)) fat32_boot_sector_t;

typedef struct {
    uint8_t filename[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) fat32_dir_entry_t;

typedef struct fat32_inode {
    uint32_t cluster;
    uint32_t size;
    uint32_t dir_cluster;
    uint32_t dir_offset;
    uint8_t is_directory;
} fat32_inode_t;


vfs_superblock_t* fat32_mount(const char *device);