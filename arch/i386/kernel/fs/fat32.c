#include "kernel/fat32.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "libc/stdio.h"
#include "libc/string.h"

static vfs_inode_t *fat32_lookup(vfs_inode_t *dir, const char *name);
static uint32_t fat32_create(vfs_inode_t *dir, const char *name, uint32_t mode);
static uint32_t fat32_unlink(vfs_inode_t *dir, const char *name);
static uint32_t fat32_read(vfs_file_t *file, void *buf, size_t count);
static uint32_t fat32_write(vfs_file_t *file, const void *buf, size_t count);
static int fat32_close(vfs_file_t *file);
static int fat32_readdir(vfs_inode_t *dir, vfs_dir_entry_t *entries, size_t max_entries);

static struct vfs_inode_operations fat32_inode_ops = {
    .lookup = fat32_lookup,
    .create = fat32_create,
    .unlink = fat32_unlink,
    .write = fat32_write,
    .read = fat32_read,
    .close = fat32_close,
    .mkdir = NULL,
    .rmdir = NULL,
    .readdir = fat32_readdir,
};

int fat32_read_cluster(fat32_superblock_t *sb, uint32_t cluster_number, void *buffer) {
    if (cluster_number < 2 || cluster_number >= (sb->total_sectors / sb->sectors_per_cluster)) {
        printf("Error: Invalid cluster number: %x\n", cluster_number);
        return -1;
    }

    uint32_t first_sector = sb->data_sector + (cluster_number - 2) * sb->sectors_per_cluster;
    uint8_t temp_buffer[sb->bytes_per_sector];

    for (uint32_t i = 0; i < sb->sectors_per_cluster; i++) {
        if (sb->device->read_block(sb->device, first_sector + i, temp_buffer) != 0) {
            printf("Error: Failed to read sector %d\n", first_sector + i);
            return -1;
        }
        memcpy((uint8_t*)buffer + (i * sb->bytes_per_sector), temp_buffer, sb->bytes_per_sector);
    }

    return 0;
}

int fat32_write_cluster(fat32_superblock_t *sb, uint32_t cluster_number, const void *buffer) {
    if (cluster_number < 2 || cluster_number >= (sb->total_sectors / sb->sectors_per_cluster)) {
        printf("Error: Invalid cluster number: %u\n", cluster_number);
        return -1;
    }

    uint32_t first_sector = sb->data_sector + (cluster_number - 2) * sb->sectors_per_cluster;
    uint8_t temp_buffer[sb->bytes_per_sector];

    for (uint32_t i = 0; i < sb->sectors_per_cluster; i++) {
        memcpy(temp_buffer, (uint8_t*)buffer + (i * sb->bytes_per_sector), sb->bytes_per_sector);
        if (sb->device->write_block(sb->device, first_sector + i, temp_buffer) != 0) {
            printf("Error: Failed to write sector %u\n", first_sector + i);
            return -1;
        }
    }

    return 0;
}

static uint32_t fat32_allocate_cluster(fat32_superblock_t *sb) {
    uint32_t fat_entries_per_sector = sb->bytes_per_sector / sizeof(uint32_t);
    uint32_t total_fat_entries = sb->fat_size * fat_entries_per_sector;
    uint32_t sector_per_fat = sb->fat_size;

    uint8_t fat_buffer[sb->bytes_per_sector];
    for (uint32_t i = 0; i < sector_per_fat; i++) {
        if (sb->device->read_block(sb->device, i, fat_buffer) != 0) {
            printf("Error: Failed to read FAT sector\n");
            return -1;
        }

        uint32_t *fat_entries = (uint32_t*)fat_buffer;
        for (uint32_t j = 0; j < fat_entries_per_sector; j++) {
            if (fat_entries[j] == FAT32_CLUSTER_FREE) {
                fat_entries[j] = FAT32_CLUSTER_LAST;
                if (sb->device->write_block(sb->device, i, fat_buffer) != 0) {
                    printf("Error: Failed to write FAT sector\n");
                    return -1;
                }

                return i * fat_entries_per_sector + j;
            }
        }
    }

    printf("Error: No free clusters available\n");
    return -1;
}

static uint32_t fat32_get_next_cluster(fat32_superblock_t *sb, uint32_t cluster) {
    uint32_t fat_offset = cluster * sizeof(uint32_t);
    uint32_t fat_sector = sb->reserved_sectors + (fat_offset / sb->bytes_per_sector);
    uint32_t offset = fat_offset % sb->bytes_per_sector;

    uint8_t buffer[512];
    if (sb->device->read_block(sb->device, fat_sector, buffer) != 0) {
        printf("Error: Failed to read FAT sector %d\n", fat_sector);
        return FAT32_CLUSTER_BAD;  // Indicate bad cluster
    }

    uint32_t next_cluster = *(uint32_t *)(buffer + offset) & 0x0FFFFFFF;
    if (next_cluster >= 0x0FFFFFF8) return FAT32_CLUSTER_LAST;

    return next_cluster; // Mask to 28 bits
}

// Set the next cluster in the FAT
static uint32_t fat32_set_next_cluster(fat32_superblock_t *sb, uint32_t cluster, uint32_t next_cluster) {
    uint32_t fat_offset = cluster * sizeof(uint32_t);
    uint32_t fat_sector = sb->reserved_sectors + (fat_offset / sb->bytes_per_sector);
    uint32_t offset = fat_offset % sb->bytes_per_sector;

    uint8_t buffer[512];
    if (sb->device->read_block(sb->device, fat_sector, buffer) != 0) {
        printf("Error: Failed to read FAT sector %d\n", fat_sector);
        return -1;
    }

    *(uint32_t *)(buffer + offset) = next_cluster & 0x0FFFFFFF;
    if (sb->device->write_block(sb->device, fat_sector, buffer) != 0) {
        printf("Error: Failed to write FAT sector %d\n", fat_sector);
        return -1;
    }

    return 0;
}

// Get the cluster number at a given offset
static uint32_t fat32_get_cluster_at_offset(fat32_superblock_t *sb, uint32_t *cluster, uint32_t offset) {
    uint32_t cluster_offset = offset / sb->cluster_size;
    for (uint32_t i = 0; i < cluster_offset; i++) {
        *cluster = fat32_get_next_cluster(sb, *cluster);
        if (*cluster == FAT32_CLUSTER_LAST) {
            printf("Error: Failed to get cluster at offset %d\n", offset);
            return -1;
        }
    }

    return 0;
}

int fat32_free_cluster_chain(fat32_superblock_t* sb, uint32_t cluster) {
    while (cluster < FAT32_CLUSTER_LAST) {
        uint32_t next_cluster = fat32_get_next_cluster(sb, cluster);
        if (next_cluster == FAT32_CLUSTER_LAST || next_cluster == 0) break;

        if (fat32_set_next_cluster(sb, cluster, FAT32_CLUSTER_FREE) != 0) {
            printf("Error: Failed to free cluster %d\n", cluster);
            return -1;
        }
        cluster = next_cluster;
    }

    return 0;
}

int fat32_read_dir_entry(fat32_superblock_t *sb, fat32_inode_t *inode, fat32_dir_entry_t *entry) {
    uint8_t buffer[sb->cluster_size];
    if (fat32_read_cluster(sb, inode->dir_cluster, buffer) != 0) return -1;

    memcpy(entry, buffer + inode->dir_offset, sizeof(fat32_dir_entry_t));
    return 0;
}

int fat32_write_dir_entry(fat32_superblock_t *sb, fat32_inode_t *inode, fat32_dir_entry_t *entry) {
    uint8_t buffer[sb->cluster_size];
    if (fat32_read_cluster(sb, inode->dir_cluster, buffer) != 0) return -1;

    memcpy(buffer + inode->dir_offset, entry, sizeof(fat32_dir_entry_t));
    if (fat32_write_cluster(sb, inode->dir_cluster, buffer) != 0) return -1;
    return 0;
}

static int fat32_prepare_entry(const char *name, fat32_dir_entry_t *entry, uint32_t mode) {
    memset(entry, 0, sizeof(fat32_dir_entry_t));
    memset(&entry->filename, ' ', 8);
    memset(&entry->ext, ' ', 3);

    int i;
    for (i = 0; i < 8 && name[i] != '.' && name[i] != '\0'; i++) {
        entry->filename[i] = toupper(name[i]);
    }
    for (int j = 0; j < 3 && name[i + 1 + j] != '\0'; j++) {
        entry->ext[j] = toupper(name[i + 1 + j]);
    }

    entry->attributes = (mode & VFS_MODE_DIR) ? FAT32_ATTR_DIRECTORY : FAT32_ATTR_ARCHIVE;
    return 0;
}

static int fat32_find_free_entry(fat32_superblock_t *sb, uint32_t cluster, fat32_dir_entry_t **entry, uint32_t *buffer) {
    while (cluster < FAT32_CLUSTER_LAST) {
        if (cluster == FAT32_CLUSTER_BAD) return -1;
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return -1;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *dir_entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (dir_entry->filename[0] == FAT32_LAST_ENTRY) {
                *entry = dir_entry;
                return 0;
            }
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    return -1;
}

static int fat32_create_entry(fat32_superblock_t *sb, fat32_dir_entry_t *entry, uint32_t mode) {
    uint32_t new_cluster = fat32_allocate_cluster(sb);
    if (new_cluster == -1) return -1;

    entry->size = 0;
    entry->first_cluster_low = new_cluster & 0xFFFF;
    entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;

    if (mode & VFS_MODE_DIR) {
        uint8_t buffer[sb->cluster_size];
        memset(buffer, 0, sb->cluster_size);
        fat32_dir_entry_t *self_entry = (fat32_dir_entry_t *)buffer;
        fat32_dir_entry_t *parent_entry = (fat32_dir_entry_t *)(buffer + sizeof(fat32_dir_entry_t));

        // '.' entry
        memcpy(self_entry, entry, sizeof(fat32_dir_entry_t));
        self_entry->filename[0] = '.';

        // '..' entry
        memset(parent_entry, 0, sizeof(fat32_dir_entry_t));
        parent_entry->filename[0] = '.';
        parent_entry->filename[1] = '.';
        parent_entry->attributes = FAT32_ATTR_DIRECTORY;

        if (fat32_write_cluster(sb, new_cluster, buffer) != 0) return -1;
    }
}

// Convert a filename to FAT32 format
void fat32_format_name(const char *name, char *out_name) {
    memset(out_name, ' ', 11);
    int i = 0;
    while (name[i] != '.' && name[i] != '\0' && i < 8) {
        out_name[i] = toupper(name[i]);
        i++;
    }

    if (name[i] == '.') {
        i++;
        int j = 0;
        while (name[i] != '\0' && j < 3) {
            out_name[8 + j] = toupper(name[i]);
            i++;
            j++;
        }
    }
}

static vfs_inode_t *fat32_lookup(vfs_inode_t *dir, const char *name) {
    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t buffer[CLUSTER_SIZE];

    char formatted_name[12];
    fat32_format_name(name, formatted_name);

    while (cluster < FAT32_CLUSTER_LAST) {
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return NULL;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (strncmp(entry->filename, formatted_name, 8) == 0 && strncmp(entry->ext, formatted_name + 8, 3) == 0) {
                vfs_inode_t *inode = (vfs_inode_t*) kmalloc(sizeof(vfs_inode_t));
                inode->mode = entry->attributes;
                inode->size = entry->size;
                inode->inode_ops = &fat32_inode_ops;
                inode->superblock = dir->superblock;

                fat32_inode_t *inode_data = (fat32_inode_t*) kmalloc(sizeof(fat32_inode_t));
                inode_data->cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
                inode_data->size = entry->size;
                inode_data->dir_offset = i * sizeof(fat32_dir_entry_t);
                inode_data->dir_cluster = cluster;

                inode->fs_data = inode_data;
                return inode;
            }
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    printf("Error: File not found:  %s\n", name);
    return NULL;
}

static uint32_t fat32_create(vfs_inode_t *dir, const char *name, uint32_t mode) {
    fat32_dir_entry_t entry;
    fat32_prepare_entry(name, &entry, mode);

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;
    uint32_t cluster = dir_inode->cluster;
    fat32_dir_entry_t *free_entry = NULL;

    uint8_t buffer[sb->cluster_size];
    if (fat32_find_free_entry(sb, cluster, &free_entry, buffer) != 0) {
        printf("Error: Failed to find free directory entry\n");
        return -1;
    }

    memcpy(free_entry, &entry, sizeof(fat32_dir_entry_t));
    if (fat32_write_cluster(sb, cluster, buffer) != 0) return -1;

    return 0;
}

static uint32_t fat32_unlink(vfs_inode_t *dir, const char* name) {
    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t buffer[sb->cluster_size];
    fat32_dir_entry_t *entry;
    int found = 0;

    char formatted_name[12];
    fat32_format_name(name, formatted_name);

    while (cluster < FAT32_CLUSTER_LAST) {
        if (fat32_read_cluster(sb, cluster, buffer) != 0) break;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (strncmp(entry->filename, formatted_name, 8) == 0 && strncmp(entry->ext, formatted_name + 8, 3) == 0) {
                found = 1;
                break;
            }
        }

        if (found) break;
        cluster = fat32_get_next_cluster(sb, cluster);
    }

    if (!found) {
        printf("Error: File not found:  %s\n", name);
        return -1;
    }

    entry->filename[0] = FAT32_DELETED_ENTRY; // Mark as deleted
    if (fat32_write_cluster(sb, cluster, buffer) != 0) return -1;

    uint32_t cluster_chain = entry->first_cluster_low | (entry->first_cluster_high << 16);
    fat32_free_cluster_chain(sb, cluster_chain);

    return 0;
}

static uint32_t fat32_write(vfs_file_t *file, const void *buf, size_t count) {
    fat32_inode_t *inode = (fat32_inode_t*)file->inode->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)file->inode->superblock->fs_data;

    uint32_t cluster = inode->cluster;
    uint32_t offset = file->offset;
    uint32_t written = 0;

    if (fat32_get_cluster_at_offset(sb, &cluster, offset) != 0) {
        printf("Error: Failed to get cluster at offset\n");
        return written;
    }

    while (written < count) {
        uint8_t buffer[sb->cluster_size];
        uint32_t cluster_offset = offset % sb->cluster_size;
        uint32_t to_write = sb->cluster_size - cluster_offset;
        if (to_write > count - written) {
            to_write = count - written;
        }

        if (cluster_offset > 0 || to_write < sb->cluster_size) {
            if (fat32_read_cluster(sb, cluster, buffer) != 0) return written;
        }

        memcpy(buffer + cluster_offset, (uint8_t*)buf + written, to_write);
        if (fat32_write_cluster(sb, cluster, buffer) != 0) break;

        written += to_write;
        offset += to_write;

        if (offset % sb->cluster_size == 0 && written < count) {
            printf("Writing to next cluster\n");
            // Allocate new cluster if needed
            uint32_t next_cluster = fat32_get_next_cluster(sb, cluster);
            if (next_cluster == FAT32_CLUSTER_LAST) {
                next_cluster = fat32_allocate_cluster(sb);
                if (next_cluster == -1) break;

                // Link the new cluster
                if (fat32_set_next_cluster(sb, cluster, next_cluster) != 0) break;
            }

            cluster = next_cluster;
        }
    }

    file->offset += written;
    if (file->offset > file->inode->size) {
        file->inode->size = file->offset;
        inode->size = file->inode->size; // Update inode's size

        fat32_dir_entry_t entry;
        if (fat32_read_dir_entry(sb, inode, &entry) == 0) {
            entry.size = file->inode->size;
            if (fat32_write_dir_entry(sb, inode, &entry) != 0) {
                printf("Error: Failed to write directory entry\n");
            }
        }
    }

    return written;
}

static uint32_t fat32_read(vfs_file_t *file, void *buf, size_t count) {
    fat32_inode_t *inode = (fat32_inode_t*)file->inode->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)file->inode->superblock->fs_data;

    uint32_t cluster = inode->cluster;
    uint32_t offset = file->offset;
    uint8_t buffer[sb->cluster_size];
    uint32_t read = 0;

    // Check if offset is within file size and adjust count
    if (offset >= file->inode->size) return 0;
    if (offset + count > file->inode->size) {
        count = file->inode->size - offset;
    }

    if (fat32_get_cluster_at_offset(sb, &cluster, offset) != 0) return read;

    while (read < count) {
        uint32_t cluster_offset = offset % sb->cluster_size;
        uint32_t to_read = sb->cluster_size - cluster_offset;
        if (to_read > count - read) {
            to_read = count - read;
        }

        if (cluster_offset > 0 || to_read < sb->cluster_size) {
            if (fat32_read_cluster(sb, cluster, buffer) != 0) return read;
        }

        memcpy((uint8_t*)buf + read, buffer + cluster_offset, to_read);

        read += to_read;
        offset += to_read;

        if (offset % sb->cluster_size == 0 && read < count) {
            cluster = fat32_get_next_cluster(sb, cluster);
            if (cluster == FAT32_CLUSTER_LAST) break;
        }
    }

    file->offset += read;
    return read;
}

static int fat32_close(vfs_file_t *file) {
    kfree(file->inode->fs_data);
    kfree(file->inode);
    kfree(file);
    return 0;
}

uint32_t fat32_entry_to_inode_number(fat32_dir_entry_t *entry, uint32_t entry_offset) {
    uint32_t start_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    return (start_cluster << 12) | entry_offset & 0xFFF;
}

static int fat32_readdir(vfs_inode_t *dir, vfs_dir_entry_t *entries, size_t max_entries) {
    if (!dir || !entries) return -1;

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t buffer[sb->cluster_size];
    uint32_t entry_count = 0;

    char lfn_buffer[256];
    size_t lfn_length = 0;

    while (cluster < FAT32_CLUSTER_LAST) {
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return -1;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (entry->filename[0] == FAT32_LAST_ENTRY) return entry_count;

            if (entry->filename[0] != FAT32_DELETED_ENTRY) {
                if (entry->attributes == FAT32_ATTR_LONG_NAME) {
                    fat32_long_dir_entry_t *lfn_entry = (fat32_long_dir_entry_t*)entry;
                    int lfn_index = (lfn_entry->order & 0x1F) - 1;
                    size_t lfn_offset = lfn_index * 13;

                    for (int j = 0; j < 13; j++) {
                        lfn_buffer[lfn_offset * 13 + j] = lfn_entry->name1[j] & 0xFF;
                    }

                }

                vfs_dir_entry_t *vfs_entry = &entries[entry_count];
                strncpy(vfs_entry->name, (char*)entry->filename, 8);
                if (entry->ext[0] != ' ') {
                    strncat(vfs_entry->name, ".", 1);
                    strncat(vfs_entry->name, (char*)entry->ext, 3);
                }

                vfs_entry->type = (entry->attributes & FAT32_ATTR_DIRECTORY) ? VFS_MODE_DIR : VFS_MODE_FILE;
                vfs_entry->inode_number = fat32_entry_to_inode_number(entry, i);

                entry_count++;
                if (entry_count >= max_entries) return entry_count;
            }
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    return entry_count;
}

static int fat32_mkdir(vfs_inode_t *dir, const char* name, uint32_t mode) {
    fat32_dir_entry_t entry;
    fat32_prepare_entry(name, &entry, mode);

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;
    uint32_t cluster = dir_inode->cluster;
    fat32_dir_entry_t *free_entry = NULL;

    uint8_t buffer[sb->cluster_size];
    if (fat32_find_free_entry(sb, cluster, &free_entry, buffer) != 0) {
        printf("Error: Failed to find free directory entry\n");
        return -1;
    }

    uint32_t new_cluster = fat32_allocate_cluster(sb);
    if (new_cluster == -1) return -1;

    entry.size = 0;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    entry.attributes = FAT32_ATTR_DIRECTORY;

    memcpy(free_entry, &entry, sizeof(fat32_dir_entry_t));
    if (fat32_write_cluster(sb, cluster, buffer) != 0) return -1;

    // Create "." and ".." entries
    fat32_dir_entry_t dot_entry;
    dot_entry.attributes = FAT32_ATTR_DIRECTORY;
    dot_entry.first_cluster_low = new_cluster & 0xFFFF;
    dot_entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;


    fat32_dir_entry_t dotdot_entry;
    dotdot_entry.attributes = FAT32_ATTR_DIRECTORY;
    dotdot_entry.first_cluster_low = dir_inode->cluster & 0xFFFF;
    dotdot_entry.first_cluster_high = (dir_inode->cluster >> 16) & 0xFFFF;

    return 0;
}

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

    fat32_boot_sector_t *boot_sector = (fat32_boot_sector_t*) buffer;

    if (boot_sector->bytes_per_sector != 512 || boot_sector->boot_signature2 != FAT32_SIGNATURE) {
        printf("Error: Invalid boot sector\n");
        return NULL;
    }

    fat32_superblock_t *sb = (fat32_superblock_t*) kmalloc(sizeof(fat32_superblock_t));
    sb->fat_count = boot_sector->fat_count;
    sb->fat_size = boot_sector->sectors_per_fat_large;
    sb->root_entries = boot_sector->root_entries;
    sb->total_sectors = boot_sector->total_sectors_large;
    sb->bytes_per_sector = boot_sector->bytes_per_sector;
    sb->sectors_per_cluster = boot_sector->sectors_per_cluster;
    sb->reserved_sectors = boot_sector->reserved_sectors;
    sb->data_sector = boot_sector->reserved_sectors + (boot_sector->fat_count * boot_sector->sectors_per_fat);
    sb->root_sector = boot_sector->root_cluster;
    sb->root_cluster = 2;
    sb->data_clusters = (boot_sector->total_sectors - sb->data_sector) / boot_sector->sectors_per_cluster;
    sb->cluster_size = boot_sector->sectors_per_cluster * boot_sector->bytes_per_sector;
    sb->device = bd;

    vfs_superblock_t *vfs_sb = (vfs_superblock_t*) kmalloc(sizeof(vfs_superblock_t));
    vfs_sb->device = bd;
    vfs_sb->fs_data = sb;

    vfs_sb->root = (vfs_inode_t*) kmalloc(sizeof(vfs_inode_t));
    vfs_sb->root->mode = VFS_MODE_DIR;
    vfs_sb->root->size = 0;
    vfs_sb->root->superblock = vfs_sb;
    vfs_sb->root->inode_ops = &fat32_inode_ops;

    fat32_inode_t *root_inode = (fat32_inode_t*) kmalloc(sizeof(fat32_inode_t));
    root_inode->cluster = sb->root_cluster;
    vfs_sb->root->fs_data = root_inode;

    return vfs_sb;
}