#pragma once

#include <stdint.h>
#include <stddef.h>

// #define MAX_OPEN_FILES 256
#define MAX_PATH_LEN 256

#define VFS_MODE_DIR 0x10
#define VFS_MODE_FILE 0x20

#define VFS_FLAG_READ 0x0
#define VFS_FLAG_WRITE 0x2
#define VFS_FLAG_APPEND 0x4
#define VFS_FLAG_CREATE 0x8
#define VFS_FLAG_TRUNC 0x10

#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 64

typedef struct {
    uint32_t mode;          // The permissions of the file
    uint32_t size;          // The size of the file
    void* fs_data;          // The filesystem-specific data
    struct vfs_inode_operations *inode_ops;
    struct vfs_superblock *superblock;
} vfs_inode_t;

typedef struct {
    int fd;                 // The file descriptor
    vfs_inode_t *inode;     // The inode of the file
    uint32_t offset;        // The offset in the file
    int flags;              // The flags of the file
    int ref_count;          // The reference count
    struct vfs_file_operations *file_ops;
} vfs_file_t;

typedef struct {
    char name[256];
    uint8_t type;           // 0 = file, 1 = directory
    uint32_t inode_number;  // 
} vfs_dir_entry_t;

struct vfs_inode_operations {
    vfs_inode_t* (*lookup)(vfs_inode_t* dir, const char* name);
    int (*create)(vfs_inode_t* dir, const char* name, uint32_t mode);
    int (*unlink)(vfs_inode_t* dir, const char* name);  // Remove a file
    int (*close)(vfs_inode_t* inode);

    uint32_t (*write)(vfs_file_t* file, const void* buf, size_t count);
    uint32_t (*read)(vfs_file_t* file, void* buf, size_t count);
    
    int (*mkdir)(vfs_inode_t* dir, const char* name, uint32_t mode);
    int (*rmdir)(vfs_inode_t* dir, const char* name);
    int (*readdir)(vfs_inode_t* dir, vfs_dir_entry_t *entries, size_t max_entries); // Read a directory entry
    int (*readdir_2)(vfs_inode_t* dir, uint32_t offset, vfs_dir_entry_t *entry); // Read a directory entry
};

struct vfs_file_operations {
    int (*read)(vfs_file_t* file, void* buf, size_t count);
    int (*write)(vfs_file_t* file, const void* buf, size_t count);
    int (*close)(vfs_file_t* file);
    int (*seek)(vfs_file_t* file, uint32_t offset, int whence);
};

typedef struct block_device {
    char name[32];      // The name of the block device
    int (*read_block)(struct block_device *dev, uint32_t block, void *buf);
    int (*write_block)(struct block_device *dev, uint32_t block, const void *buf);
    void *device_data;  // Device-specific data (e.g., file descriptor or memory pointer)
} block_device_t;

typedef struct vfs_superblock {
    vfs_inode_t *root;      // The root inode of the filesystem
    block_device_t *device; // The block device
    void *fs_data;          // The filesystem-specific data
} vfs_superblock_t;

typedef struct vfs_mount{
    const char * path;       // The path where the filesystem is mounted
    vfs_superblock_t *sb;    // The superblock of the filesystem
    struct vfs_mount *next;       // The next mount in the list
} vfs_mount_t;

typedef struct {
    const char * name;      // The name of the filesystem
    vfs_superblock_t *(*mount)(const char *device); // The mount function
} vfs_fs_type_t;


vfs_file_t *vfs_get_file(int fd);
block_device_t *get_block_device(const char *path);
int register_block_device(block_device_t *dev);


int vfs_relative_path(const char *cwd, const char *path, char *resolved_path);

int vfs_mount(const char *path, vfs_superblock_t *sb);
int vfs_unmount(const char *path);
int vfs_create(const char *path, uint32_t mode);
int vfs_unlink(const char *path);
vfs_file_t *vfs_open(const char *path, int flags);
int vfs_close(vfs_file_t *file);

int vfs_write(vfs_file_t *file, const void *buf, size_t count);
int vfs_read(vfs_file_t *file, void *buf, size_t count);

int vfs_seek(vfs_file_t *file, uint32_t offset, int whence);
int vfs_rmdir(const char *path);
int vfs_readdir(int fd, vfs_dir_entry_t *entries, size_t max_entries);
int vfs_getdents(int fd, void *buf, int size);

int vfs_register_fs_type(vfs_fs_type_t *fs_type);

void vfs_init();