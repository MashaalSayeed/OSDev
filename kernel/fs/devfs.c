#include "kernel/vfs.h"
#include "kernel/printf.h"
#include "kernel/kheap.h"
#include "libc/string.h"
#include "kernel/devfs.h"

static vfs_device_t device_table[32] = {0};
static int device_count = 0;

static vfs_inode_t *devfs_root = NULL;

static vfs_inode_t *devfs_lookup(vfs_inode_t *dir, const char *name) {
    printf("reached lookup: %s\n", name);
    if (!dir || !name) return NULL;

    for (int i = 0; i < device_count; i++) {
        if (strcmp(device_table[i].name, name) == 0) {
            vfs_inode_t *inode = (vfs_inode_t*)kmalloc(sizeof(vfs_inode_t));
            memset(inode, 0, sizeof(vfs_inode_t));
            inode->mode = (device_table[i].type == DEVICE_TYPE_CHAR) ? VFS_MODE_FILE : VFS_MODE_DIR;
            inode->size = 0;
            inode->fs_data = &device_table[i];
            inode->inode_ops = dir->inode_ops;
            return inode;
        }
    }

    return NULL;
}

static int devfs_readdir(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry) {
    if (!dir || !entry) return -1;
    if (offset >= device_count) return 0;

    vfs_device_t *dev = &device_table[offset];
    strncpy(entry->name, dev->name, sizeof(entry->name));
    entry->type = (dev->type == DEVICE_TYPE_CHAR) ? 0 : 1;
    entry->inode_number = offset + 1;

    return offset + 1;
}

static int devfs_close(vfs_inode_t *inode) {
    if (!inode || inode == devfs_root) return -1;
    kfree(inode);
    return 0;
}

static uint32_t devfs_read(vfs_file_t *file, void *buf, size_t count) {
    if (!file || !buf || count == 0) return -1;

    vfs_device_t *dev = (vfs_device_t *)file->inode->fs_data;
    if (dev->type != DEVICE_TYPE_CHAR || !dev->char_dev.read_char) {
        return -1;
    }

    return dev->char_dev.read_char(dev, buf, count);
}

static uint32_t devfs_write(vfs_file_t *file, const void *buf, size_t count) {
    if (!file || !buf || count == 0) return -1;

    vfs_device_t *dev = (vfs_device_t *)file->inode->fs_data;
    if (dev->type != DEVICE_TYPE_CHAR || !dev->char_dev.write_char) {
        return -1;
    }

    return dev->char_dev.write_char(dev, buf, count);
}

static struct vfs_inode_operations devfs_inode_ops = {
    .lookup = devfs_lookup,
    .readdir = devfs_readdir,
    .create = NULL,
    .unlink = NULL,
    .close = devfs_close,
    .write = devfs_write,
    .read = devfs_read,
    .mkdir = NULL,
    .rmdir = NULL
};

int devfs_register_device(vfs_device_t *device) {
    if (device_count >= 32) {
        printf("Error: Device table full\n");
        return -1;
    }

    device_table[device_count++] = *device;
    return 0;
}

vfs_device_t *devfs_get_device(const char *name) {
    for (int i = 0; i < device_count; i++) {
        if (strcmp(device_table[i].name, name) == 0) {
            return &device_table[i];
        }
    }
    return NULL;
}

vfs_superblock_t *devfs_mount(vfs_device_t *device) {
    vfs_superblock_t *sb = (vfs_superblock_t*)kmalloc(sizeof(vfs_superblock_t));
    memset(sb, 0, sizeof(vfs_superblock_t));

    devfs_root = (vfs_inode_t*)kmalloc(sizeof(vfs_inode_t));
    memset(devfs_root, 0, sizeof(vfs_inode_t));

    devfs_root->mode = VFS_MODE_DIR;
    devfs_root->size = 0;
    devfs_root->inode_ops = &devfs_inode_ops;

    sb->root = devfs_root;
    return sb;
}
