#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/ramfs.h"
#include "libc/string.h"
#include "libc/stdio.h"

vfs_file_t* vfs_fd_table[MAX_OPEN_FILES];
vfs_mount_t* vfs_mount_list = NULL;
vfs_mount_t* root_mount = NULL;

int vfs_mount(const char *path, vfs_superblock_t *sb) {
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

static vfs_inode_t * resolve_path(const char *path, vfs_mount_t **mount_out) {
    if (path[0] != '/') {
        printf("Error: Path must start with '/'\n");
        return NULL;
    }

    vfs_mount_t *mount = find_mount(path);
    if (!mount) return NULL;

    *mount_out = mount;

    vfs_inode_t *current_inode = mount->sb->root;

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");

    while (token) {
        current_inode = current_inode->inode_ops->lookup(current_inode, token);
        if (!current_inode) {
            kfree(path_copy);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    kfree(path_copy);
    return current_inode;
}

int vfs_open(const char *path, int flags) {
    vfs_mount_t *mount;
    vfs_inode_t *inode = resolve_path(path, &mount);

    if (!mount || !inode) {
        printf("Error: Failed to resolve path: %s\n", path);
        return -1;
    }

    // printf("Resolved path: %s\n", path);
    // vfs_inode_t *dir = mount->sb->root;

    // Find an available file descriptor
    int fd;
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (!vfs_fd_table[fd]) break;
    }

    vfs_file_t* file = (vfs_file_t *)kmalloc(sizeof(vfs_file_t));
    file->fd = fd;
    file->inode = inode;
    file->offset = 0;
    file->flags = flags;

    vfs_fd_table[fd] = file;

    return fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !vfs_fd_table[fd]) return -1;
    vfs_file_t *file = vfs_fd_table[fd];
    // file->inode->inode_ops->close(file);

    kfree(file);
    vfs_fd_table[fd] = NULL;
    return 0;
}

uint32_t vfs_write(int fd, const void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !vfs_fd_table[fd]) return -1;
    vfs_file_t *file = vfs_fd_table[fd];

    return file->inode->inode_ops->write(file, buf, count);
}

uint32_t vfs_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !vfs_fd_table[fd]) return -1;
    vfs_file_t *file = vfs_fd_table[fd];

    return file->inode->inode_ops->read(file, buf, count);
}

int vfs_register_fs_type(vfs_fs_type_t *fs_type) {
    return 0;
}

void test_vfs(vfs_superblock_t *root_sb) {
    vfs_inode_t* root = root_sb->root;
    // vfs_mount_t* root_node = root->fs_data;
    // printf("Root inode: %s\n", root_node->path);
    root->inode_ops->create(root, "example.txt", 0644);
    root->inode_ops->create(root, "init.log", 0644);

    int fd = vfs_open("/example.txt", 0);
    printf("Opened file: %d\n", fd);
    vfs_write(fd, "Hello, World!\n", 14);
    printf("Writing to file\n");
    vfs_close(fd);

    fd = vfs_open("/example.txt", 0);
    char buf[32];
    int count = vfs_read(fd, buf, 32);
    buf[count] = '\0';
    printf("Read from file: %s\n", buf);
}

void vfs_init() {
    memset(vfs_fd_table, 0, sizeof(vfs_fd_table));
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

    if (vfs_mount("/", sb) != 0) {
        printf("Failed to mount at /\n");
        return;
    }

    test_vfs(sb);
}