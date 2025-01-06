#pragma once

#include <stdint.h>
#include "kernel/vfs.h"

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    block_device_t *block_device;
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
} __attribute__((packed)) fat32_directory_entry_t;


vfs_superblock_t* fat32_mount(const char *device);