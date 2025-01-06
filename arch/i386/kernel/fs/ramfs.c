#include "kernel/ramfs.h"
#include "kernel/kheap.h"
#include "libc/string.h"
#include "libc/stdio.h"

ramfs_node_t *ramfs_root;

static struct vfs_inode_operations ramfs_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .write = ramfs_write,
    .read = ramfs_read,
    .close = ramfs_close,
    .mkdir = ramfs_mkdir,
};

static ramfs_node_t* ramfs_find_child(ramfs_node_t* parent, const char* name) {
    ramfs_node_t* child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0)
            return child;
        child = child->next;
    }
    return NULL;
}

static vfs_inode_t* ramfs_lookup(vfs_inode_t *dir, const char *name) {
    ramfs_node_t *node = (ramfs_node_t *)dir->fs_data;
    ramfs_node_t *child = ramfs_find_child(node, name);

    if (!child) return NULL;

    vfs_inode_t* inode = kmalloc(sizeof(vfs_inode_t));
    inode->mode = child->mode;
    inode->size = child->size;
    inode->fs_data = child;
    inode->inode_ops = &ramfs_inode_ops;
    return inode;
}

static int ramfs_create(vfs_inode_t *dir, const char *name, uint32_t mode) {
    if (!dir || !(dir->mode & VFS_MODE_DIR) || !name) return -1;

    ramfs_node_t *parent = (ramfs_node_t *)dir->fs_data;
    ramfs_node_t *node = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));

    memset(node, 0, sizeof(ramfs_node_t));
    strcpy(node->name, name);
    node->mode = mode;
    node->parent = parent;

    ramfs_node_t *child = parent->children;
    if (!child) {
        parent->children = node;
    } else {
        while (child->next) {
            child = child->next;
        }
        child->next = node;
    }

    return 0;
}

static uint32_t ramfs_write(vfs_file_t *file, const void *buf, size_t count) {
    ramfs_node_t *node = (ramfs_node_t *)file->inode->fs_data;
    if (!node->data) {
        node->data = kmalloc(count);
        node->size = 0;
    } else if (file->offset + count > node->size) {
        // node->data = krealloc(node->data, node->size + count);
    }

    memcpy((char *)node->data + file->offset, buf, count);
    file->offset += count;
    if (file->offset > node->size) {
        node->size = file->offset;
    }

    return count;
}

static uint32_t ramfs_read(vfs_file_t *file, void *buf, size_t count) {
    ramfs_node_t *node = (ramfs_node_t *)file->inode->fs_data;

    if (file->offset + count > node->size) {
        count = node->size - file->offset;
    }

    memcpy(buf, (char *)node->data + file->offset, count);
    file->offset += count;
    return count;
}

static int ramfs_close(vfs_file_t *file) {
    kfree(file->inode);
    kfree(file);
    return 0;
}

static int ramfs_mkdir(vfs_inode_t *dir, const char *name, uint32_t mode) {
    if (!dir || !name) return -1;

    // Check if the directory already exists
    if (ramfs_lookup(dir, name)) {
        return -1;
    }

    ramfs_node_t *parent = (ramfs_node_t *)dir->fs_data;
    ramfs_node_t *node = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));

    memset(node, 0, sizeof(ramfs_node_t));
    strcpy(node->name, name);
    node->mode = VFS_MODE_DIR | mode;
    node->parent = parent;

    ramfs_node_t *child = parent->children;
    if (!child) {
        parent->children = node;
    } else {
        while (child->next) {
            child = child->next;
        }
        child->next = node;
    }

    return 0;
}


vfs_superblock_t *ramfs_mount(const char *device) {
    ramfs_root = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));
    if (!ramfs_root) {
        return NULL;
    }

    memset(ramfs_root, 0, sizeof(ramfs_node_t));
    strcpy(ramfs_root->name, "/");
    ramfs_root->mode = 0x755;

    vfs_superblock_t *sb = (vfs_superblock_t *)kmalloc(sizeof(vfs_superblock_t));
    if (!sb) {
        kfree(ramfs_root);
        return NULL;
    }

    sb->root = kmalloc(sizeof(vfs_inode_t));
    sb->root->mode = VFS_MODE_DIR | ramfs_root->mode;
    sb->root->size = 0;
    sb->root->fs_data = ramfs_root;
    sb->root->inode_ops = &ramfs_inode_ops;
    sb->fs_data = NULL;

    return sb;
}