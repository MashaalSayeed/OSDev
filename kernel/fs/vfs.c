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
#include "kernel/devfs.h"


#define MAX_BLOCK_DEVICES 16

vfs_mount_t* vfs_mount_list = NULL;
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

int console_write(vfs_file_t* file, const void* buf, size_t count) {
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

// Smallest prefix match for mount point
static vfs_mount_t *find_mount(const char *path) {
    vfs_mount_t *best = NULL;
    size_t best_len = 0;

    for (vfs_mount_t *m = vfs_mount_list; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0) {
            if (mlen > best_len) {
                best = m;
                best_len = mlen;
            }
        }
    }

    return best;
}

int vfs_mount(const char *path, vfs_superblock_t *sb) {
    // Check if the mount path already exists
    vfs_mount_t *mount = find_mount(path);
    if (mount && strncmp(mount->path, path, strlen(path)) == 0) {
        printf("Error: FS already mounted: %s\n", path);
        return -1;
    }

    mount = (vfs_mount_t*) kmalloc(sizeof(vfs_mount_t));
    if (!mount) return -1;

    mount->path = path;
    mount->sb = sb;
    mount->next = vfs_mount_list;
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
    if (!*relative_path || strcmp(relative_path, "/") == 0) return current_inode; // Root directory

    char *path_copy = strdup(relative_path);
    char *token = strtok(path_copy, "/");
    while (token) {
        vfs_inode_t *next_inode = current_inode->inode_ops->lookup(current_inode, token);
        if (!next_inode) {
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
    vfs_mount_t *mount;
    vfs_inode_t *inode = resolve_path(path, &mount);

    if (!inode) {
        if (flags & O_CREAT) {
            int ret = vfs_create(path, VFS_MODE_FILE);
            if (ret < 0) {
                printf("Error: Failed to create file: %s\n", path);
                return NULL;
            }

            inode = resolve_path(path, &mount);
            if (!inode) {
                printf("Error: Failed to resolve newly created file: %s\n", path);
                return NULL;
            }
        } else {
            printf("Error: Failed to resolve path: %s\n", path);
            return NULL;
        }
    }
    
    vfs_file_t* file = (vfs_file_t *)kmalloc(sizeof(vfs_file_t));
    if (!file) return NULL;

    file->inode = inode;
    file->offset = 0;
    file->flags = flags;
    file->ref_count = 1;

    file->file_ops = &vfs_default_file_ops;
    return file;
}

int vfs_close(vfs_file_t *file) {
    if (!file) return -1;

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

int vfs_write(vfs_file_t *file, const void* buf, size_t count) {
    if (!file || file->inode->mode != VFS_MODE_FILE) {
        printf("Error: Not a file\n");
        return -1;
    }

    return file->inode->inode_ops->write(file, buf, count);
}

int vfs_read(vfs_file_t *file, void* buf, size_t count) {
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

int vfs_getdents(int fd, void* buf, int size) {
    vfs_file_t *file = vfs_get_file(fd);
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file || !buf) return -1;

    if (file->inode->mode != VFS_MODE_DIR || file->inode->inode_ops->readdir == NULL) {
        printf("Error: Not a directory %d\n", file->inode->mode);
        return -1;
    }

    int bytes_written = 0;
    uint32_t offset = file->offset;
    vfs_dir_entry_t entry;

    while (bytes_written + sizeof(linux_dirent_t) <= size) {
        if ((offset = file->inode->inode_ops->readdir(file->inode, offset, &entry)) <= 0) break;

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

void init_fs(vfs_fs_type_t *fs_type, vfs_device_t *device, const char *mount_path) {
    if (vfs_register_fs_type(fs_type) != 0) {
        printf("Failed to register filesystem: %s\n", fs_type->name);
        return;
    }

    vfs_superblock_t *sb = fs_type->mount(device);
    if (!sb) {
        printf("Failed to mount root fs: %s\n", fs_type->name);
        return;
    }

    if (vfs_mount(mount_path, sb) != 0) {
        printf("Failed to mount at %s\n", mount_path);
        return;
    }
}

static vfs_fs_type_t ramfs = { .name = "ramfs", .mount = ramfs_mount };
static vfs_fs_type_t fat32 = { .name = "fat32", .mount = fat32_mount };
static vfs_fs_type_t devfs = { .name = "devfs", .mount = devfs_mount };

void vfs_init() {
    console_init();

    init_fs(&devfs, NULL, "/dev");
    init_fs(&fat32, devfs_get_device("sda1"), "/");
    init_fs(&ramfs, NULL, "/ram");

    if (vfs_create("/home", VFS_MODE_DIR) != 0) {
        printf("Failed to create directory: /home\n");
        return;
    }
}