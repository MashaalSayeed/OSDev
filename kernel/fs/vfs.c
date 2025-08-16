#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/ramfs.h"
#include "kernel/fat32.h"
#include "kernel/printf.h"
#include "kernel/process.h"
#include "user/dirent.h"
#include "kernel/hashtable.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "kernel/string.h"
#include <drivers/keyboard.h>
#include <drivers/tty.h>


#define MAX_BLOCK_DEVICES 16

static block_device_t* block_device_table[MAX_BLOCK_DEVICES] = {0};

// vfs_file_t* vfs_fd_table[MAX_OPEN_FILES] = {0};
vfs_mount_t* vfs_mount_list = NULL;
vfs_mount_t* root_mount = NULL;
hash_table_t vfs_table = {0};

struct vfs_file_operations vfs_default_file_ops = {
    .read = vfs_read,
    .write = vfs_write,
    .close = vfs_close,
    .seek = vfs_seek,
};

vfs_file_t* console_fds[3];

vfs_file_t* vfs_get_file(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return NULL;
    if (fd < 3) return console_fds[fd];
    
    process_t *proc = get_current_process();
    return proc->fds[fd];
}

int console_read(vfs_file_t* file, void* buf, size_t count) {
    return read_keyboard_buffer(buf, count);
}

int console_write(vfs_file_t* file, void* buf, size_t count) {
    // printf("%d", count);
    terminal_write(buf, count);
    return count;
}

struct vfs_file_operations vfs_console_ops = {
    .read = console_read,
    .write = console_write,
    .close = vfs_close,
    .seek = NULL,
};

void console_init() {
    console_fds[0] = (vfs_file_t*) kmalloc(sizeof(vfs_file_t));
    console_fds[0]->fd = 0;
    console_fds[0]->file_ops = &vfs_console_ops;
    console_fds[0]->flags = O_RDONLY;
    console_fds[0]->ref_count = 1;

    console_fds[1] = (vfs_file_t*) kmalloc(sizeof(vfs_file_t));
    console_fds[1]->fd = 1;
    console_fds[1]->file_ops = &vfs_console_ops;
    console_fds[1]->flags = O_WRONLY;
    console_fds[1]->ref_count = 1;

    console_fds[2] = (vfs_file_t*) kmalloc(sizeof(vfs_file_t));
    console_fds[2]->fd = 2;
    console_fds[2]->file_ops = &vfs_console_ops;
    console_fds[2]->flags = O_WRONLY;
    console_fds[2]->ref_count = 1;
}

int register_block_device(block_device_t *device) {
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        if (!block_device_table[i]) {
            block_device_table[i] = device;
            return 0;
        }
    }
    return -1; // No space left to register the device
}

block_device_t *get_block_device(const char *device) {
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        if (block_device_table[i] && strcmp(device, block_device_table[i]->name) == 0) {
            return block_device_table[i];
        }
    }
    return NULL;
}

void simplify_path(char *path) {
    char stack[MAX_PATH_LEN][MAX_PATH_LEN];
    int top = 0;
    char *saveptr;
    char temp[MAX_PATH_LEN];
    int i = 0;

    strcpy(temp, path);

    char *token = strtok_r(temp, "/", &saveptr);
    while (token) {
        // if (i++ == 0) token++;  // Skip the leading slash
        if (strcmp(token, "..") == 0) {
            if (top > 0) top--;  // Go up one directory
        } else if (strcmp(token, ".") != 0 && strcmp(token, "") != 0) {
            strcpy(stack[top++], token);  // Store a copy of the token
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    // Rebuild the normalized path
    path[0] = '\0';  // Clear the path

    if (top == 0) {
        strcpy(path, "/");  // Root case
    } else {
        for (int i = 0; i < top; i++) {
            strcat(path, "/");
            strcat(path, stack[i]);
        }
    }
}

int vfs_relative_path(const char *cwd, const char *path, char *out) {
    if (path[0] == '/') {  
        // Absolute path
        strncpy(out, path, MAX_PATH_LEN);
    } else if (strcmp(cwd, "/") == 0) {
        // Root directory
        snprintf(out, MAX_PATH_LEN, "/%s", path);
    } else {
        // Relative path: concatenate with `cwd`
        snprintf(out, MAX_PATH_LEN, "%s/%s", cwd, path);
    }

    simplify_path(out);
    return 1;
}

int split_path(const char* path, char** dirname, char** filename) {
    if (!path || !dirname || !filename) return -1;

    char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        *dirname = strdup("/");
        *filename = strdup(path);
    } else {
        if (last_slash == path) {
            *dirname = strdup("/");
        } else {
            *dirname = strndup(path, last_slash - path);
        }

        *filename = strdup(last_slash + 1);
    }

    return 0;
}

// currently just supports single mount "/"
static vfs_mount_t * find_mount(const char *path) {
    vfs_mount_t * mount = vfs_mount_list;
    while (mount) {
        size_t mount_path_len = strlen(mount->path);
        if (strncmp(path, mount->path, mount_path_len) == 0) return mount;
        mount = mount->next;
    }

    return NULL;
}

int vfs_mount(const char *path, vfs_superblock_t *sb) {
    // Check if the mount path already exists
    if (find_mount(path)) {
        printf("Error: FS already mounted: %s\n", path);
        return -1;
    }

    vfs_mount_t *mount = (vfs_mount_t*) kmalloc(sizeof(vfs_mount_t));
    if (!mount) {
        return -1;
    }

    mount->path = path;
    mount->sb = sb;
    mount->next = vfs_mount_list;

    if (!vfs_mount_list) root_mount = mount;

    vfs_mount_list = mount;
    return 0;
}

int vfs_unmount(const char *path) {
    vfs_mount_t *mount = vfs_mount_list;
    vfs_mount_t *prev = NULL;

    while (mount) {
        if (strcmp(mount->path, path) == 0) {
            if (prev) {
                prev->next = mount->next;
            } else {
                vfs_mount_list = mount->next;
            }

            kfree(mount);
            return 0;
        }

        prev = mount;
        mount = mount->next;
    }

    return -1;
}

static vfs_inode_t * resolve_path(const char *path, vfs_mount_t **mount_out) {
    if (path[0] != '/') {
        printf("Error: Path must start with '/'. Got: %s\n", path);
        return NULL;
    }

    vfs_mount_t *mount = find_mount(path);
    if (!mount) {
        printf("Error: Failed to find mount for path: %s\n", path);
        return NULL;
    }

    *mount_out = mount;
    const char* relative_path = path + strlen(mount->path); // Skip the mount path
    if (*relative_path == '/') relative_path++; // Skip the leading slash

    vfs_inode_t *current_inode = mount->sb->root;

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    while (token) {
        vfs_inode_t *next_inode = current_inode->inode_ops->lookup(current_inode, token);
        if (!current_inode) {
            kfree(path_copy);
            return NULL;
        }

        current_inode->inode_ops->close(current_inode);
        current_inode = next_inode;
        token = strtok(NULL, "/");
    }

    kfree(path_copy);
    return current_inode;
}

vfs_inode_t *vfs_traverse(const char *path) {
    vfs_mount_t *mount;
    return resolve_path(path, &mount);
}

vfs_file_t *vfs_open(const char *path, int flags) {
    process_t *proc = get_current_process();
    vfs_mount_t *mount;
    vfs_inode_t *inode = resolve_path(path, &mount);

    if (!inode) {
        if (flags & O_CREAT) {
            int ret = vfs_create(path, VFS_MODE_FILE);
            if (ret < 0) {
                printf("Error: Failed to create file: %s\n", path);
                return -1;
            }

            inode = resolve_path(path, &mount);
            if (!inode) {
                printf("Error: Failed to resolve newly created file: %s\n", path);
                return -1;
            }
        } else {
            printf("Error: Failed to resolve path: %s\n", path);
            return -1;
        }
    }

    // Find an available file descriptor
    int fd;
    for (fd = 3; fd < MAX_OPEN_FILES; fd++) {
        if (!proc->fds[fd]) break;
    }

    if (fd == MAX_OPEN_FILES) {
        printf("Error: No available file descriptors\n");
        return -1;
    }

    vfs_file_t* file = (vfs_file_t *)kmalloc(sizeof(vfs_file_t));
    if (!file) return -1;

    file->fd = fd;
    file->inode = inode;
    file->offset = 0;
    file->flags = flags;
    file->ref_count = 1;

    file->file_ops = &vfs_default_file_ops;

    proc->fds[fd] = file;
    return file;
}

int vfs_close(vfs_file_t *file) {
    if (!file) return -1;
    process_t *proc = get_current_process();
    proc->fds[file->fd] = NULL;

    if (--file->ref_count > 0) return 0;

    if (file->inode && file->inode->inode_ops->close) {
        file->inode->inode_ops->close(file->inode);
    }

    kfree(file);
    return 0;
}

int vfs_create(const char *path, uint32_t mode) {
    if (!path) return -1;

    // Split the path into the directory and the file name
    char *dirname, *filename;
    if (split_path(path, &dirname, &filename) != 0) {
        printf("Error: Failed to split path: %s\n", path);
        return -1;
    }

    vfs_inode_t *dir = vfs_traverse(dirname);
    kfree(dirname);

    if (!dir) {
        printf("Error: Failed to resolve path: %s\n", path);
        kfree(filename);
        return -1;
    }

    int result = dir->inode_ops->create(dir, filename, mode);
    kfree(filename);
    return result;
}

int vfs_unlink(const char *path) {
    char *dirname, *filename;
    split_path(path, &dirname, &filename);

    vfs_inode_t *dir = vfs_traverse(dirname);
    kfree(dirname);

    if (!dir) {
        printf("Error: Failed to resolve path: %s\n", path);
        kfree(filename);
        return -1;
    }

    int result = dir->inode_ops->unlink(dir, filename);
    kfree(filename);
    return result;
}

uint32_t vfs_write(vfs_file_t *file, const void* buf, size_t count) {
    if (!file || file->inode->mode != VFS_MODE_FILE) {
        printf("Error: Not a file\n");
        return -1;
    }

    return file->inode->inode_ops->write(file, buf, count);
}

uint32_t vfs_read(vfs_file_t *file, void* buf, size_t count) {
    if (!file || file->inode->mode != VFS_MODE_FILE) {
        printf("Error: Not a file\n");
        return -1;
    }

    // if (!(file->flags & VFS_FLAG_READ)) {
    //     printf("Error: File not opened for reading %d\n");
    //     return -1;
    // }

    memset(buf, 0, count);
    return file->inode->inode_ops->read(file, buf, count);
}

int vfs_seek(vfs_file_t *file, uint32_t offset, int whence) {
    if (!file) return -1;

    int new_offset = file->offset;
    switch (whence) {
        case VFS_SEEK_SET:
            new_offset = offset;
            break;
        case VFS_SEEK_CUR:
            new_offset += offset;
            break;
        case VFS_SEEK_END:
            new_offset = file->inode->size + offset;
            break;
        default:
            return -1;
    }

    if (new_offset < 0) return -1;

    file->offset = new_offset;
    return 0;
}

int vfs_rmdir(const char *path) {
    char *dirname, *filename;
    split_path(path, &dirname, &filename);

    vfs_inode_t *dir = vfs_traverse(dirname);
    kfree(dirname);
    if (!dir) {
        printf("Error: Failed to resolve path: %s\n", path);
        kfree(filename);
        return -1;
    }

    int result = dir->inode_ops->rmdir(dir, filename);
    kfree(filename);
    return result;
}

int vfs_readdir(int fd, vfs_dir_entry_t *entries, size_t max_entries) {
    vfs_file_t *file = vfs_get_file(fd);
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file || !entries) return -1;

    if (file->inode->mode != VFS_MODE_DIR) {
        printf("Error: Not a directory\n");
        return -1;
    }

    return file->inode->inode_ops->readdir(file->inode, entries, max_entries);
}

int vfs_getdents(int fd, void* buf, int size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file || !buf) return -1;

    if (file->inode->mode != VFS_MODE_DIR) {
        printf("Error: Not a directory %d\n", file->inode->mode);
        return -1;
    }

    int bytes_written = 0;
    uint32_t offset = file->offset;
    vfs_dir_entry_t entry;

    while (bytes_written + sizeof(linux_dirent_t) <= size) {
        if ((offset = file->inode->inode_ops->readdir_2(file->inode, offset, &entry)) <= 0) break;

        linux_dirent_t *dirp = (linux_dirent_t*)((uint8_t*)buf + bytes_written);
        dirp->d_ino = entry.inode_number;
        dirp->d_off = offset;
        dirp->d_reclen = sizeof(linux_dirent_t);
        dirp->d_type = entry.type;

        strncpy(dirp->d_name, entry.name, sizeof(dirp->d_name) - 1);
        dirp->d_name[sizeof(dirp->d_name) - 1] = '\0';

        bytes_written += sizeof(linux_dirent_t);
    }

    file->offset = offset;
    return bytes_written;
}

int vfs_register_fs_type(vfs_fs_type_t *fs_type) {
    return hash_set(&vfs_table, fs_type->name, fs_type);
}

int vfs_unregister_fs_type(const char *name) {
    return hash_remove(&vfs_table, name);
}

vfs_fs_type_t *vfs_get_fs_type(const char *name) {
    return (vfs_fs_type_t*)hash_get(&vfs_table, name);
}

void test_vfs(vfs_superblock_t *root_sb) {
    // linux_dirent_t entries[16];
    // int bytes_read = vfs_getdents(fd, entries, sizeof(entries));
    // printf("Read %d bytes\n", bytes_read);
    // for (int i = 0; i < bytes_read / sizeof(linux_dirent_t); i++) {
    //     printf("Inode: %d, Name: %s, Type: %d\n", entries[i].d_ino, entries[i].d_name, entries[i].d_type);
    // }
    vfs_file_t *file = vfs_open("/", VFS_FLAG_READ);

    vfs_dir_entry_t entries[16];
    int file_count = vfs_readdir(file->fd, &entries, sizeof(entries) / sizeof(vfs_dir_entry_t));
    vfs_close(file);
    printf("Files in root (%d):\n", file_count);
    for (int i = 0; i < file_count; i++) {
        if (entries[i].type == VFS_MODE_DIR) {
            printf(">  %s\n", entries[i].name);
        } else {
            printf("   %s\n", entries[i].name);
        }
    }

    // int fd = vfs_open("/sofa.txt", VFS_FLAG_READ);
    // int fd = vfs_open("/k", VFS_FLAG_READ);

    // char buffer[256];
    // int bytes_read = vfs_read(fd, buffer, sizeof(buffer));
    // printf("Read %d bytes: %s\n", bytes_read, buffer);
    // vfs_close(fd);
}

void ramfs_init() {
    vfs_fs_type_t ramfs = {
        .name = "ramfs",
        .mount = ramfs_mount
    };

    if (vfs_register_fs_type(&ramfs) != 0) {
        printf("Failed to register filesystem: ramfs\n");
        return;
    }

    vfs_superblock_t *sb = ramfs.mount(NULL);
    if (!sb) {
        printf("Failed to mount root fs: ramfs\n");
        return;
    }

    if (vfs_mount("/ram", sb) != 0) {
        printf("Failed to mount at /ram\n");
        return;
    }
}

void fat32_init() {
    block_device_t *ata = get_block_device("/dev/sda1");
    if (!ata) {
        printf("ATA block device not found\n");
        return;
    }

    vfs_fs_type_t fat32 = {
        .name = "fat32",
        .mount = fat32_mount
    };
    if (vfs_register_fs_type(&fat32) != 0) {
        printf("Failed to register filesystem: fat32\n");
        return;
    }

    vfs_superblock_t *sb = fat32.mount("/dev/sda1");
    if (!sb) {
        printf("Failed to mount root fs: fat32\n");
        return;
    }

    if (vfs_mount("/", sb) != 0) {
        printf("Failed to mount at /\n");
        return;
    }

    int fd;
    if (vfs_create("/home", VFS_MODE_DIR) != 0) {
        printf("Failed to create directory: /home\n");
        // return;
    }

    // printf("fd: %d, file: %x\n", fd, vfs_get_file(fd));
    // if (vfs_create("/tmp", VFS_MODE_DIR) != 0) {
    //     printf("Failed to create directory: /tmp\n");
    //     return;
    // }
    // test_vfs(sb);
}

void vfs_init() {
    block_device_t *ata = get_block_device("/dev/sda1");
    if (!ata) {
        printf("ATA block device not found\n");
        return;
    }

    console_init();
    fat32_init();
    // ramfs_init();
}