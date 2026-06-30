#include "kernel/ramfs.h"
#include "kernel/kheap.h"
#include "libc/string.h"

static int ramfs_readdir(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry);

static struct vfs_inode_operations ramfs_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .write = ramfs_write,
    .read = ramfs_read,
    .close = ramfs_close,
    .mkdir = ramfs_mkdir,
    .readdir = ramfs_readdir,
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
    inode->superblock = dir->superblock;
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
    size_t new_size = file->offset + count;

    if (!node->data) {
        node->data = kmalloc(new_size);
    } else if (new_size > node->size) {
        void *new_data = kmalloc(new_size);
        memcpy(new_data, node->data, node->size);
        kfree(node->data);
        node->data = new_data;
    }

    memcpy((char *)node->data + file->offset, buf, count);
    file->offset += count;
    if (file->offset > node->size) node->size = file->offset;
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

static int ramfs_close(vfs_inode_t *inode) {
    if (!inode || (inode->superblock && inode == inode->superblock->root)) return -1;
    kfree(inode); // don't touch inode->fs_data, the node lives in the tree
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

static int ramfs_readdir(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry) {
    ramfs_node_t *node = (ramfs_node_t *)dir->fs_data;
    ramfs_node_t *child = node->children;

    uint32_t index = 0;
    while (child) {
        if (index >= offset) {
            strncpy(entry->name, child->name, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            entry->type = (child->mode & VFS_MODE_DIR) ? VFS_MODE_DIR : VFS_MODE_FILE;
            entry->inode_number = (uint32_t)(uintptr_t)child;
            return index + 1;
        }
        index++;
        child = child->next;
    }

    return 0;
}

vfs_superblock_t *ramfs_mount(const char *device) {
    ramfs_node_t *root = (ramfs_node_t *)kmalloc(sizeof(ramfs_node_t));
    if (!root) return NULL;

    memset(root, 0, sizeof(ramfs_node_t));
    strcpy(root->name, "/");
    root->mode = VFS_MODE_DIR | 0x755;

    vfs_superblock_t *sb = (vfs_superblock_t *)kmalloc(sizeof(vfs_superblock_t));
    if (!sb) {
        kfree(root);
        return NULL;
    }

    vfs_inode_t *inode = kmalloc(sizeof(vfs_inode_t));
    if (!inode) {
        kfree(root);
        kfree(sb);
        return NULL;
    }

    inode->mode = VFS_MODE_DIR | 0x755;
    inode->size = 0;
    inode->fs_data = root;
    inode->inode_ops = &ramfs_inode_ops;
    inode->superblock = sb;

    sb->root = inode;
    sb->fs_data = root; // keep a reference for unmount/cleanup later
    sb->device = NULL;

    return sb;
}