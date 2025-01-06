#include "kernel/fat32.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"

vfs_superblock_t * fat32_mount(const char *device) {
    block_device_t *bd = get_block_device(device);
    if (!bd) {
        printf("Error: Block device not found: %s\n", device);
        return NULL;
    }

    uint8_t buffer[512];
    if (bd->read_block(bd, 0, buffer) != 0) {
        printf("Error: Failed to read boot sector\n");
        return NULL;
    }
    
    // // Read the boot sector
    // fat32_boot_sector_t *boot = (fat32_boot_sector_t *)kmalloc(sizeof(fat32_boot_sector_t));
    // read_block(0, (uint8_t *)boot);
    
    // // Create the superblock
    // vfs_superblock_t *sb = (vfs_superblock_t *)kmalloc(sizeof(vfs_superblock_t));
    // sb->fs_data = boot;
    
    // // Create the root inode
    // vfs_inode_t *root = (vfs_inode_t *)kmalloc(sizeof(vfs_inode_t));
    // root->mode = VFS_MODE_DIR;
    // root->size = 0;
    // root->fs_data = boot;
    // root->inode_ops = (vfs_inode_operations_t *)kmalloc(sizeof(vfs_inode_operations_t));
    // root->inode_ops->lookup = fat32_lookup;
    // root->inode_ops->create = fat32_create;
    // root->inode_ops->unlink = fat32_unlink;
    // root->inode_ops->write = fat32_write;
    // root->inode_ops->read = fat32_read;
    // root->inode_ops->close = fat32_close;
    // root->inode_ops->mkdir = fat32_mkdir;
    // root->inode_ops->rmdir = fat32_rmdir;
    // root->inode_ops->readdir = fat32_readdir;
    
    // // Set the root inode
    // sb->root = root;
    
    // return sb;
}