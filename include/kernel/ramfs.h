#pragma once

#include <stdint.h>
#include "kernel/vfs.h"

typedef struct ramfs_node {
    char name[128];               // Name of the file or directory
    uint32_t mode;                // Permissions and type
    uint32_t size;                // Size of the file (in bytes)
    void *data;                   // Pointer to the file's data (for regular files)
    struct ramfs_node *parent;    // Parent directory
    struct ramfs_node *children;  // First child (if directory)
    struct ramfs_node *next;      // Next sibling
} ramfs_node_t;


static vfs_inode_t* ramfs_lookup(vfs_inode_t* dir, const char* name);
static int ramfs_create(vfs_inode_t* dir, const char* name, uint32_t mode);
static uint32_t ramfs_write(vfs_file_t* file, const void* buf, size_t count);
static uint32_t ramfs_read(vfs_file_t* file, void* buf, size_t count);
static int ramfs_close(vfs_file_t* file);

static int ramfs_mkdir(vfs_inode_t* dir, const char* name, uint32_t mode);
static int ramfs_rmdir(vfs_inode_t* dir, const char* name);
static vfs_inode_t* ramfs_readdir(vfs_inode_t* dir, uint32_t index);

vfs_superblock_t* ramfs_mount(const char* path, block_device_t *block_device);
