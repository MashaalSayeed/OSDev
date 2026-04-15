#pragma once

#include <stdint.h>

#define DT_UNKNOWN 0
#define DT_REG 1
#define DT_DIR 2

typedef struct linux_dirent {
    uint32_t d_ino;         // Inode number
    uint32_t d_off;         // Offset to next entry
    uint16_t d_reclen;      // Record length
    uint8_t  d_type;        // File type (DT_REG, DT_DIR, etc.)
    char     d_name[256];   // File name (null-terminated)
} linux_dirent_t;

typedef struct __attribute__((packed)) linux_dirent64 {
    uint64_t d_ino;         // Inode number
    int64_t  d_off;         // Offset to next entry
    uint16_t d_reclen;      // Record length
    uint8_t  d_type;        // File type (DT_REG, DT_DIR, etc.)
    char     d_name[];      // File name (null-terminated)
} linux_dirent64_t;