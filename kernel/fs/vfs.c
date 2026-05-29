#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/ramfs.h"
#include "kernel/fat32.h"
#include "kernel/devfs.h"
#include "kernel/printf.h"
#include "kernel/process.h"
#include "common/dirent.h"
#include "common/ioctl.h"
#include "common/signals.h"
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
    .ioctl = NULL,
};

static int devfs_file_read(vfs_file_t* file, void* buf, size_t count);
static int devfs_file_write(vfs_file_t* file, const void* buf, size_t count);
static int devfs_file_seek(vfs_file_t* file, uint32_t offset, int whence);
static int devfs_file_ioctl(vfs_file_t* file, int request, void *arg);

static struct vfs_file_operations vfs_devfs_file_ops = {
    .read = devfs_file_read,
    .write = devfs_file_write,
    .close = vfs_close,
    .seek = devfs_file_seek,
    .ioctl = devfs_file_ioctl,
};

static termios_linux_t g_tty_termios = {
    .c_iflag = 0,
    .c_oflag = 0,
    .c_cflag = 0,
    .c_lflag = TTY_LFLAG_ISIG | TTY_LFLAG_ICANON | TTY_LFLAG_ECHO,
    .c_line = 0,
    .c_cc = {
        [VINTR] = 3,
        [VQUIT] = 28,
        [VERASE] = '\b',
        [VKILL] = 21,
        [VEOF] = 4,
        [VTIME] = 0,
        [VMIN] = 1,
        [VSTART] = 17,
        [VSTOP] = 19,
        [VSUSP] = 26,
        [VEOL] = '\n'
    }
};

static winsize_linux_t g_tty_winsize = {
    .ws_row = 25,
    .ws_col = 80,
    .ws_xpixel = 0,
    .ws_ypixel = 0
};

#define TTY_LINEBUF_SIZE 512
static char g_tty_linebuf[TTY_LINEBUF_SIZE];
static size_t g_tty_linebuf_len = 0;
static size_t g_tty_linebuf_pos = 0;

static void tty_echo_char(char c) {
    if (!(g_tty_termios.c_lflag & TTY_LFLAG_ECHO)) return;

    if (c == '\b' || c == 0x7F) {
        terminal_write("\b \b", 3);
    } else {
        terminal_write(&c, 1);
    }
}

static int tty_handle_signal_char(char c) {
    if (!(g_tty_termios.c_lflag & TTY_LFLAG_ISIG)) return 0;

    process_t *proc = get_current_process();
    if (!proc) return 0;

    if (c == (char)g_tty_termios.c_cc[VINTR]) {
        proc->pending_signals |= (1u << SIGINT);
        if (g_tty_termios.c_lflag & TTY_LFLAG_ECHO) terminal_write("^C\n", 3);
        return 1;
    }

    if (c == (char)g_tty_termios.c_cc[VSUSP]) {
        proc->pending_signals |= (1u << SIGTSTP);
        if (g_tty_termios.c_lflag & TTY_LFLAG_ECHO) terminal_write("^Z\n", 3);
        return 1;
    }

    return 0;
}

static int tty_fill_canonical_line(void) {
    g_tty_linebuf_len = 0;
    g_tty_linebuf_pos = 0;

    while (g_tty_linebuf_len < TTY_LINEBUF_SIZE - 1) {
        char c = kgetch();

        if (tty_handle_signal_char(c)) continue;

        if (c == (char)g_tty_termios.c_cc[VERASE] || c == '\b' || c == 0x7F) {
            if (g_tty_linebuf_len > 0) {
                g_tty_linebuf_len--;
                tty_echo_char('\b');
            }
            continue;
        }

        if (c == (char)g_tty_termios.c_cc[VKILL]) {
            while (g_tty_linebuf_len > 0) {
                g_tty_linebuf_len--;
                tty_echo_char('\b');
            }
            continue;
        }

        if (c == (char)g_tty_termios.c_cc[VEOF]) {
            return (g_tty_linebuf_len == 0) ? 0 : (int)g_tty_linebuf_len;
        }

        g_tty_linebuf[g_tty_linebuf_len++] = c;
        tty_echo_char(c);

        if (c == '\n' || c == (char)g_tty_termios.c_cc[VEOL]) break;
    }

    g_tty_linebuf[g_tty_linebuf_len] = '\0';
    return (int)g_tty_linebuf_len;
}

static int tty_read_common(char *buf, size_t count) {
    if (!buf || count == 0) return -1;

    if (g_tty_termios.c_lflag & TTY_LFLAG_ICANON) {
        if (g_tty_linebuf_pos >= g_tty_linebuf_len) {
            int line_status = tty_fill_canonical_line();
            if (line_status <= 0) return line_status;
        }

        size_t available = g_tty_linebuf_len - g_tty_linebuf_pos;
        size_t to_copy = (count < available) ? count : available;
        memcpy(buf, g_tty_linebuf + g_tty_linebuf_pos, to_copy);
        g_tty_linebuf_pos += to_copy;

        if (g_tty_linebuf_pos >= g_tty_linebuf_len) {
            g_tty_linebuf_pos = 0;
            g_tty_linebuf_len = 0;
        }

        return (int)to_copy;
    }

    size_t bytes_read = 0;
    int vmin = g_tty_termios.c_cc[VMIN];

    if (vmin == 0) {
        while (bytes_read < count) {
            int ch = keyboard_try_getchar();
            if (ch < 0) break;

            char c = (char)ch;
            if (tty_handle_signal_char(c)) continue;
            tty_echo_char(c);
            buf[bytes_read++] = c;
        }
        return (int)bytes_read;
    }

    while (bytes_read < (size_t)vmin && bytes_read < count) {
        char c = kgetch();
        if (tty_handle_signal_char(c)) continue;
        tty_echo_char(c);
        buf[bytes_read++] = c;
    }

    while (bytes_read < count) {
        int ch = keyboard_try_getchar();
        if (ch < 0) break;

        char c = (char)ch;
        if (tty_handle_signal_char(c)) continue;
        tty_echo_char(c);
        buf[bytes_read++] = c;
    }

    return (int)bytes_read;
}

static int tty_common_ioctl(int request, void *arg) {
    if (!arg) return -1;

    switch (request) {
        case TCGETS:
            memcpy(arg, &g_tty_termios, sizeof(g_tty_termios));
            return 0;
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
            memcpy(&g_tty_termios, arg, sizeof(g_tty_termios));
            return 0;
        case TIOCGWINSZ: {
            winsize_linux_t *ws = (winsize_linux_t *)arg;
            ws->ws_row = (uint16_t)terminal_get_height();
            ws->ws_col = (uint16_t)terminal_get_width();
            ws->ws_xpixel = g_tty_winsize.ws_xpixel;
            ws->ws_ypixel = g_tty_winsize.ws_ypixel;
            return 0;
        }
        case TIOCSWINSZ:
            memcpy(&g_tty_winsize, arg, sizeof(g_tty_winsize));
            return 0;
        default:
            return -1;
    }
}

vfs_file_t* console_fds[3];

vfs_file_t* vfs_get_file(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return NULL;
    if (fd < 3) return console_fds[fd];
    
    process_t *proc = get_current_process();
    return proc->fds[fd];
}

int console_read(vfs_file_t* file, void* buf, size_t count) {
    return tty_read_common((char *)buf, count);
}

int console_write(vfs_file_t* file, const void* buf, size_t count) {
    terminal_write(buf, count);
    return count;
}

int console_ioctl(vfs_file_t* file, int request, void *arg) {
    return tty_common_ioctl(request, arg);
}

static int tty0_dev_read(struct vfs_device *dev, char *buf, size_t count) {
    return tty_read_common(buf, count);
}

static int tty0_dev_write(struct vfs_device *dev, const char *buf, size_t count) {
    terminal_write(buf, count);
    return count;
}

static int tty0_dev_ioctl(struct vfs_device *dev, int request, void *arg) {
    return tty_common_ioctl(request, arg);
}

static int null_dev_read(struct vfs_device *dev, char *buf, size_t count) {
    (void)dev;
    (void)buf;
    (void)count;
    return 0;
}

static int null_dev_write(struct vfs_device *dev, const char *buf, size_t count) {
    (void)dev;
    (void)buf;
    return (int)count;
}

static int zero_dev_read(struct vfs_device *dev, char *buf, size_t count) {
    (void)dev;
    if (!buf || count == 0) return 0;
    memset(buf, 0, count);
    return (int)count;
}

static int zero_dev_write(struct vfs_device *dev, const char *buf, size_t count) {
    (void)dev;
    (void)buf;
    return (int)count;
}

static int devfs_file_read(vfs_file_t* file, void* buf, size_t count) {
    if (!file || !file->inode || !file->inode->fs_data || !buf) return -1;

    vfs_device_t *dev = (vfs_device_t *)file->inode->fs_data;
    if (dev->type != DEVICE_TYPE_CHAR || !dev->char_dev.read_char) return -1;

    return dev->char_dev.read_char(dev, buf, count);
}

static int devfs_file_write(vfs_file_t* file, const void* buf, size_t count) {
    if (!file || !file->inode || !file->inode->fs_data || !buf) return -1;

    vfs_device_t *dev = (vfs_device_t *)file->inode->fs_data;
    if (dev->type != DEVICE_TYPE_CHAR || !dev->char_dev.write_char) return -1;

    return dev->char_dev.write_char(dev, buf, count);
}

static int devfs_file_seek(vfs_file_t* file, uint32_t offset, int whence) {
    return -1;
}

static int devfs_file_ioctl(vfs_file_t* file, int request, void *arg) {
    if (!file || !file->inode || !file->inode->fs_data) return -1;

    vfs_device_t *dev = (vfs_device_t *)file->inode->fs_data;
    if (dev->type != DEVICE_TYPE_CHAR || !dev->char_dev.ioctl) return -1;

    return dev->char_dev.ioctl(dev, request, arg);
}

struct vfs_file_operations vfs_console_ops = {
    .read = console_read,
    .write = console_write,
    .close = vfs_close,
    .seek = NULL,
    .ioctl = console_ioctl,
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

// Supports nested mounts - finds the longest matching mount path
static vfs_mount_t * find_mount(const char *path) {
    vfs_mount_t * mount = vfs_mount_list;
    vfs_mount_t * best_match = NULL;
    size_t best_match_len = 0;

    while (mount) {
        size_t mount_path_len = strlen(mount->path);
        if (strncmp(path, mount->path, mount_path_len) == 0) {
            // Check if this is the longest matching mount so far
            if (mount_path_len > best_match_len) {
                best_match = mount;
                best_match_len = mount_path_len;
            }
        }
        mount = mount->next;
    }

    return best_match;
}

int vfs_mount(const char *path, vfs_superblock_t *sb) {
    // Check if the mount path already exists
    vfs_mount_t *existing_mount = find_mount(path);
    if (existing_mount && strcmp(existing_mount->path, path) == 0) {
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
    process_t *proc = get_current_process();
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

    file->fd = -1; //fd;
    file->inode = inode;
    file->offset = 0;
    file->flags = flags;
    file->ref_count = 1;

    if (mount && strcmp(mount->path, "/DEV") == 0) {
        file->file_ops = &vfs_devfs_file_ops;
    } else {
        file->file_ops = &vfs_default_file_ops;
    }
    return file;
}

int vfs_close(vfs_file_t *file) {
    if (!file) return -1;
    if (--file->ref_count > 0) return 0;
    if (file->inode && file->inode->inode_ops && file->inode->inode_ops->close) {
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

int vfs_rename(const char *oldpath, const char *newpath) {
    char *old_dirname, *old_filename;
    char *new_dirname, *new_filename;

    split_path(oldpath, &old_dirname, &old_filename);
    split_path(newpath, &new_dirname, &new_filename);

    vfs_inode_t *old_dir = vfs_traverse(old_dirname);
    vfs_inode_t *new_dir = vfs_traverse(new_dirname);

    kfree(old_dirname);
    kfree(new_dirname);

    if (!old_dir || !new_dir) {
        kfree(old_filename);
        kfree(new_filename);
        return -1;
    }

    if (!old_dir->inode_ops->rename) {
        kfree(old_filename);
        kfree(new_filename);
        return -1;
    }

    int ret = old_dir->inode_ops->rename(old_dir, old_filename, new_dir, new_filename);
    kfree(old_filename);
    kfree(new_filename);
    return ret;
}

int vfs_write(vfs_file_t *file, const void* buf, size_t count) {
    if (!file || file->inode->mode != VFS_MODE_FILE) {
        printf("Error: Not a file\n");
        return -1;
    }

    if (((file->flags & O_WRONLY) == 0) && ((file->flags & O_RDWR) == 0)) {
        printf("Error: File not opened for writing\n");
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

    if (file->inode->mode != VFS_MODE_DIR) {
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

void init_fs(vfs_fs_type_t *fs_type, const char *device, const char *mount_path) {
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
        printf("Failed to mount %s at %s\n", fs_type->name, mount_path);
        return;
    }
}

void vfs_init() {
    block_device_t *ata = get_block_device("/dev/sda1");
    if (!ata) {
        printf("ATA block device not found\n");
        return;
    }

    console_init();

    vfs_fs_type_t fat32_fs_type = {
        .name = "fat32",
        .mount = fat32_mount
    };

    vfs_fs_type_t ramfs_fs_type = {
        .name = "ramfs",
        .mount = ramfs_mount
    };

    init_fs(&fat32_fs_type, "/dev/sda1", "/");
    init_fs(&ramfs_fs_type, NULL, "/mnt/ramfs");

    /* Mount devfs at /DEV so that devices appear as /DEV/<NAME> */
    vfs_superblock_t *devfs_sb = devfs_mount(NULL);
    if (devfs_sb) {
        if (vfs_mount("/DEV", devfs_sb) != 0)
            printf("Failed to mount devfs at /DEV\n");
        else
            printf("devfs mounted at /DEV\n");

        vfs_device_t tty0 = {0};
        strncpy(tty0.name, "TTY0", DEVFS_NAME_LEN - 1);
        tty0.name[DEVFS_NAME_LEN - 1] = '\0';
        tty0.type = DEVICE_TYPE_CHAR;
        tty0.char_dev.read_char = tty0_dev_read;
        tty0.char_dev.write_char = tty0_dev_write;
        tty0.char_dev.ioctl = tty0_dev_ioctl;

        if (devfs_register_device(&tty0) != 0)
            printf("Failed to register /DEV/TTY0\n");
        else
            printf("Registered /DEV/TTY0\n");

        vfs_device_t dev_null = {0};
        strncpy(dev_null.name, "NULL", DEVFS_NAME_LEN - 1);
        dev_null.name[DEVFS_NAME_LEN - 1] = '\0';
        dev_null.type = DEVICE_TYPE_CHAR;
        dev_null.char_dev.read_char = null_dev_read;
        dev_null.char_dev.write_char = null_dev_write;

        if (devfs_register_device(&dev_null) != 0)
            printf("Failed to register /DEV/NULL\n");
        else
            printf("Registered /DEV/NULL\n");

        vfs_device_t dev_zero = {0};
        strncpy(dev_zero.name, "ZERO", DEVFS_NAME_LEN - 1);
        dev_zero.name[DEVFS_NAME_LEN - 1] = '\0';
        dev_zero.type = DEVICE_TYPE_CHAR;
        dev_zero.char_dev.read_char = zero_dev_read;
        dev_zero.char_dev.write_char = zero_dev_write;

        if (devfs_register_device(&dev_zero) != 0)
            printf("Failed to register /DEV/ZERO\n");
        else
            printf("Registered /DEV/ZERO\n");
    } else {
        printf("Failed to create devfs superblock\n");
    }

    int fd;
    if (vfs_create("/home", VFS_MODE_DIR) != 0) {
        printf("Failed to create directory: /home\n");
        return;
    }
}