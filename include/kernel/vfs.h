#pragma once

#include <stdint.h>
#include <stddef.h>

#define MAX_OPEN_FILES 256

typedef struct vfs_inode {
    uint32_t mode;          // The permissions of the file
    uint32_t size;          // The size of the file
    void* fs_data;          // The filesystem-specific data
    struct vfs_inode_operations *inode_ops;
} vfs_inode_t;

typedef struct vfs_file {
    int fd;                 // The file descriptor
    vfs_inode_t *inode;     // The inode of the file
    uint32_t offset;        // The offset in the file
    int flags;              // The flags of the file
} vfs_file_t;

struct vfs_inode_operations {
    vfs_inode_t* (*lookup)(vfs_inode_t* dir, const char* name);
    int (*create)(vfs_inode_t* dir, const char* name, uint32_t mode);
    int (*unlink)(vfs_inode_t* dir, const char* name);
    int (*mkdir)(vfs_inode_t* dir, const char* name, uint32_t mode);
    uint32_t (*write)(vfs_file_t* file, const void* buf, size_t count);
    uint32_t (*read)(vfs_file_t* file, void* buf, size_t count);
};

struct vfs_file_operations {
    uint32_t (*read)(vfs_file_t* file, void* buf, size_t count);
    uint32_t (*write)(vfs_file_t* file, const void* buf, size_t count);
    int (*close)(vfs_file_t* file);
    uint32_t (*seek)(vfs_file_t* file, uint32_t offset, int whence);
};

typedef struct vfs_superblock {
    vfs_inode_t *root;      // The root inode of the filesystem
    void *fs_data;          // The filesystem-specific data
} vfs_superblock_t;

typedef struct vfs_mount {
    const char * path;       // The path where the filesystem is mounted
    vfs_superblock_t *sb;    // The superblock of the filesystem
    struct vfs_mount *next;       // The next mount in the list
} vfs_mount_t;

typedef struct vfs_fs_type {
    const char * name;      // The name of the filesystem
    vfs_superblock_t *(*mount)(const char *device); // The mount function
} vfs_fs_type_t;

int vfs_mount(const char *path, vfs_superblock_t *sb);
int vfs_unmount(const char *path);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);

uint32_t vfs_write(int fd, const void *buf, size_t count);
uint32_t vfs_read(int fd, void *buf, size_t count);

int vfs_register_fs_type(vfs_fs_type_t *fs_type);

void vfs_init();