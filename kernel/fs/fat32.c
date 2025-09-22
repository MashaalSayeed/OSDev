#include "kernel/fat32.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "libc/string.h"

static vfs_inode_t *fat32_lookup(vfs_inode_t *dir, const char *name);
static int fat32_create(vfs_inode_t *dir, const char *name, uint32_t mode);
static int fat32_unlink(vfs_inode_t *dir, const char *name);
static uint32_t fat32_read(vfs_file_t *file, void *buf, size_t count);
static uint32_t fat32_write(vfs_file_t *file, const void *buf, size_t count);
static int fat32_close(vfs_inode_t *inode);
static int fat32_readdir(vfs_inode_t *dir, vfs_dir_entry_t *entries, size_t max_entries);
static int fat32_readdir_2(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry);

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
    .readdir_2 = fat32_readdir_2
};

int fat32_read_cluster(fat32_superblock_t *sb, uint32_t cluster_number, void *buffer) {
    if (cluster_number < 2 || cluster_number >= (sb->total_sectors / sb->sectors_per_cluster)) {
        printf("[fat32_read_cluster] Error: Invalid cluster number: %x\n", cluster_number);
        return -1;
    }

    uint32_t first_sector = sb->data_sector + (cluster_number - 2) * sb->sectors_per_cluster;
    uint8_t temp_buffer[512];

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
        printf("[fat32_write_cluster] Error: Invalid cluster number: %u\n", cluster_number);
        return -1;
    }

    uint32_t first_sector = sb->data_sector + (cluster_number - 2) * sb->sectors_per_cluster;
    uint8_t temp_buffer[512];

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

    uint8_t fat_buffer[512];
    for (uint32_t i = 0; i < sector_per_fat; i++) {
        if (sb->device->read_block(sb->device, i, fat_buffer) != 0) {
            printf("Error: Failed to read FAT sector\n");
            return -1;
        }

        uint32_t *fat_entries = (uint32_t*)fat_buffer;
        for (uint32_t j = 0; j < fat_entries_per_sector; j++) {
            uint32_t cluster = i * fat_entries_per_sector + j;
            // Skip reserved clusters (host OS specific metadata)
            if (cluster < 0x10 || cluster >= total_fat_entries) continue;

            if (fat_entries[j] == FAT32_CLUSTER_FREE) {
                fat_entries[j] = FAT32_CLUSTER_LAST;
                if (sb->device->write_block(sb->device, i, fat_buffer) != 0) {
                    printf("Error: Failed to write FAT sector\n");
                    return -1;
                }
                return cluster;
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

    uint8_t *buffer = kmalloc(sb->bytes_per_sector);
    if (sb->device->read_block(sb->device, fat_sector, buffer) != 0) {
        printf("Error: Failed to read FAT sector %d\n", fat_sector);
        kfree(buffer);
        return FAT32_CLUSTER_BAD;  // Indicate bad cluster
    }

    uint32_t next_cluster = *(uint32_t *)(buffer + offset) & 0x0FFFFFFF;
    kfree(buffer);

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
    memset(entry->filename, ' ', 8); // Fill with spaces
    memset(entry->ext, ' ', 3);      // Fill with spaces

    int i = 0, j = 0;
    int name_length = strlen(name);
    int dot_pos = -1;

    // Find the '.' for file extension (if any)
    for (int k = 0; k < name_length; k++) {
        if (name[k] == '.') {
            dot_pos = k;
            break;
        }
    }

    if (dot_pos == -1) { // No extension
        dot_pos = name_length; // Treat whole name as filename
    }

    // Copy filename (up to 8 characters)
    for (i = 0; i < 8 && i < dot_pos; i++) {
        entry->filename[i] = name[i];
    }

    // Copy extension if it exists
    if (dot_pos < name_length - 1) { // If there's something after the dot
        for (j = 0; j < 3 && (dot_pos + 1 + j) < name_length; j++) {
            entry->ext[j] = name[dot_pos + 1 + j];
        }
    }

    // Set attributes correctly
    if (mode & VFS_MODE_DIR) {
        entry->attributes = FAT32_ATTR_DIRECTORY;
    } else {
        entry->attributes = FAT32_ATTR_ARCHIVE;
    }

    return 0;
}

static int fat32_find_free_entry(fat32_superblock_t *sb, uint32_t cluster, fat32_dir_entry_t **entry, uint8_t *buffer) {
    while (cluster < FAT32_CLUSTER_LAST) {
        if (cluster == FAT32_CLUSTER_BAD) return -1;
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return -1;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *dir_entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (dir_entry->filename[0] == FAT32_LAST_ENTRY || dir_entry->filename[0] == FAT32_DELETED_ENTRY) {
                *entry = dir_entry;
                return 0;
            }
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    // TODO: Allocate a new cluster if no free entries found
    return -1;
}

static int fat32_is_directory_empty(fat32_superblock_t *sb, uint32_t cluster) {
    uint8_t buffer[sb->cluster_size];
    
    while (cluster < FAT32_CLUSTER_LAST) {
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return 0;

        fat32_dir_entry_t *entry = (fat32_dir_entry_t*)buffer;
        for (int i = 2; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) { 
            if (entry[i].filename[0] != FAT32_DELETED_ENTRY && entry[i].filename[0] != 0) {
                // printf("Entry: %s\n", entry[i].filename);
                return 0; // Directory is not empty
            }
        }
        cluster = fat32_get_next_cluster(sb, cluster);
    }
    return 1; // Directory is empty
}

// Convert a filename to FAT32 format
void fat32_format_name(const char *name, char *out_name) {
    memset(out_name, ' ', 11);
    int i = 0;
    while (name[i] != '.' && name[i] != '\0' && i < 8) {
        out_name[i] = name[i];
        i++;
    }

    if (name[i] == '.') {
        i++;
        int j = 0;
        while (name[i] != '\0' && j < 3) {
            out_name[8 + j] = name[i];
            i++;
            j++;
        }
    }
}

static int fat32_find_entry(fat32_superblock_t *sb, uint32_t cluster, const char *name, fat32_dir_entry_t **entry, uint8_t *buffer) {
    while (cluster < FAT32_CLUSTER_LAST) {
        if (cluster == FAT32_CLUSTER_BAD) return -1;
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return -1;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *dir_entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (dir_entry->filename[0] == FAT32_LAST_ENTRY) return -1;
            if (dir_entry->filename[0] == FAT32_DELETED_ENTRY) continue;

            char formatted_name[12];
            fat32_format_name(name, formatted_name);

            if (strncmp(dir_entry->filename, formatted_name, 8) == 0 && strncmp(dir_entry->ext, formatted_name + 8, 3) == 0) {
                *entry = dir_entry;
                return 0;
            }
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    return -1;
}

static vfs_inode_t *fat32_lookup(vfs_inode_t *dir, const char *name) {
    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t *buffer = kmalloc(sb->cluster_size);
    char formatted_name[12];
    fat32_format_name(name, formatted_name);

    while (cluster < FAT32_CLUSTER_LAST) {
        if (cluster == FAT32_CLUSTER_BAD || cluster == FAT32_CLUSTER_FREE) return NULL;
        if (fat32_read_cluster(sb, cluster, buffer) != 0) {
            kfree(buffer);
            return NULL;
        }
        
        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            
            if (entry->filename[0] == FAT32_DELETED_ENTRY) continue;

            if (strncmp(entry->filename, formatted_name, 8) == 0 && strncmp(entry->ext, formatted_name + 8, 3) == 0) {
                vfs_inode_t *inode = (vfs_inode_t*) kmalloc(sizeof(vfs_inode_t));
                if (!inode) {
                    kfree(buffer);
                    return NULL;
                }

                inode->mode = entry->attributes;
                inode->size = entry->size;
                inode->inode_ops = &fat32_inode_ops;
                inode->superblock = dir->superblock;

                fat32_inode_t *inode_data = (fat32_inode_t*) kmalloc(sizeof(fat32_inode_t));
                if (!inode_data) {
                    kfree(inode);
                    kfree(buffer);
                    return NULL;
                }

                inode_data->cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
                inode_data->size = entry->size;
                inode_data->dir_offset = i * sizeof(fat32_dir_entry_t);
                inode_data->dir_cluster = cluster;

                inode->fs_data = inode_data;
                kfree(buffer);
                return inode;
            }
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    kfree(buffer);
    return NULL;
}

static int fat32_create(vfs_inode_t *dir, const char *name, uint32_t mode) {
    fat32_dir_entry_t entry;
    fat32_prepare_entry(name, &entry, mode);

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;
    uint32_t cluster = dir_inode->cluster;
    fat32_dir_entry_t *free_entry = NULL;

    uint8_t buffer[sb->cluster_size];
    if (fat32_find_entry(sb, cluster, name, &free_entry, buffer) == 0) {
        printf("Error: File already exists: %s\n", name);
        return -1;
    }

    if (fat32_find_free_entry(sb, cluster, &free_entry, buffer) != 0) {
        printf("Error: Failed to find free directory entry\n");
        return -1;
    }

    uint32_t new_cluster = fat32_allocate_cluster(sb);
    if (new_cluster == (uint32_t)-1) return -1;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    
    fat32_set_next_cluster(sb, new_cluster, FAT32_CLUSTER_LAST);
    
    // Write the new entry to the parent directory
    memcpy(free_entry, &entry, sizeof(fat32_dir_entry_t));
    if (fat32_write_cluster(sb, cluster, buffer) != 0) return -1;

    if (mode & VFS_MODE_DIR) {
        // Initialize "." and ".." entries
        uint8_t new_buffer[sb->cluster_size];
        memset(new_buffer, 0, sb->cluster_size);

        fat32_dir_entry_t *dot_entry = (fat32_dir_entry_t *)new_buffer;
        memcpy(dot_entry->filename, ".          ", 11);
        dot_entry->attributes = FAT32_ATTR_DIRECTORY; 
        dot_entry->first_cluster_low = new_cluster & 0xFFFF;
        dot_entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;

        fat32_dir_entry_t *dotdot_entry = (fat32_dir_entry_t *)(new_buffer + sizeof(fat32_dir_entry_t));
        memcpy(dotdot_entry->filename, "..         ", 11);
        dotdot_entry->attributes = FAT32_ATTR_DIRECTORY;
        if (dir_inode->cluster == sb->root_cluster) {
            dotdot_entry->first_cluster_low = 0;
            dotdot_entry->first_cluster_high = 0;
        } else {
            dotdot_entry->first_cluster_low = dir_inode->cluster & 0xFFFF;
            dotdot_entry->first_cluster_high = (dir_inode->cluster >> 16) & 0xFFFF;
        }

        // Write the "." and ".." entries to the newly allocated cluster
        if (fat32_write_cluster(sb, new_cluster, new_buffer) != 0) return -1;
    }

    return 0;
}

static int fat32_unlink(vfs_inode_t *dir, const char* name) {
    if (!dir || !name || name[0] == '\0') return -1;

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t buffer[sb->cluster_size];
    fat32_dir_entry_t *entry = NULL;

    if (fat32_find_entry(sb, cluster, name, &entry, buffer) != 0) {
        printf("Error: File not found: %s\n", name);
        return -1;
    }

    uint32_t cluster_chain = entry->first_cluster_low | (entry->first_cluster_high << 16);
    if (entry->attributes & FAT32_ATTR_DIRECTORY) {
        if (!fat32_is_directory_empty(sb, cluster_chain)) {
            printf("Error: Directory is not empty: %s\n", name);
            return -1;
        }
    }

    memset(entry, 0, sizeof(fat32_dir_entry_t));
    entry->filename[0] = FAT32_DELETED_ENTRY; // Mark as deleted
    if (fat32_write_cluster(sb, cluster, buffer) != 0) return -1;

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

    while (written < count && cluster != FAT32_CLUSTER_LAST) {
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
    uint32_t read = 0;
    
    // Check if offset is within file size and adjust count
    if (offset >= file->inode->size) return 0;
    if (offset + count > file->inode->size) {
        count = file->inode->size - offset;
    }
    
    if (fat32_get_cluster_at_offset(sb, &cluster, offset) != 0) return read;
    uint8_t *buffer = kmalloc(sb->cluster_size);
    
    while (read < count && cluster != FAT32_CLUSTER_LAST) {
        uint32_t cluster_offset = offset % sb->cluster_size;
        uint32_t to_read = sb->cluster_size - cluster_offset;
        if (to_read > count - read) {
            to_read = count - read;
        }

        if (fat32_read_cluster(sb, cluster, buffer) != 0) {
            kfree(buffer);
            return read;
        }
        memcpy((uint8_t*)buf + read, buffer + cluster_offset, to_read);

        read += to_read;
        offset += to_read;

        if (offset % sb->cluster_size == 0 && read < count) {
            cluster = fat32_get_next_cluster(sb, cluster);
        }
    }

    file->offset += read;
    kfree(buffer);
    return read;
}

static int fat32_close(vfs_inode_t *inode) {
    if (!inode || inode == inode->superblock->root) return -1;
    kfree(inode->fs_data);
    kfree(inode);
    return 0;
}

uint32_t fat32_entry_to_inode_number(fat32_dir_entry_t *entry, uint32_t entry_offset) {
    uint32_t start_cluster = (entry->first_cluster_high << 16) | entry->first_cluster_low;
    return (start_cluster << 12) | entry_offset & 0xFFF;
}

char fat32_extract_lfn_char(fat32_long_dir_entry_t *entry, int index) {
    char c = '\0';
    if (index < 5) {
        c = entry->name1[index]; // First 5 characters (Unicode, skip high byte)
    } else if (index < 11) {
        c = entry->name2[(index - 5)]; // Next 6 characters
    } else if (index < 13) {
        c = entry->name3[(index - 11)]; // Last 2 characters
    }
    return (c < 128) ? c : '?'; // Out of range
}

static int fat32_readdir(vfs_inode_t *dir, vfs_dir_entry_t *entries, size_t max_entries) {
    if (!dir || !entries) return -1;

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t buffer[sb->cluster_size];
    uint32_t entry_count = 0;

    char lfn_buffer[256] = {0};
    size_t lfn_length = 0;
    int lfn_order = -1;

    while (cluster < FAT32_CLUSTER_LAST) {
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return -1;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));
            if (entry->filename[0] == FAT32_LAST_ENTRY) return entry_count;
            if (entry->filename[0] == FAT32_DELETED_ENTRY) continue;

            if (entry->attributes == FAT32_ATTR_LONG_NAME) {
                fat32_long_dir_entry_t *lfn_entry = (fat32_long_dir_entry_t*)entry;
                int entry_order = lfn_entry->order & 0x1F;

                if (lfn_order == -1 || entry_order == lfn_order - 1) {
                    // Append characters in correct order
                    for (int j = 0; j < 13; j++) {
                        char c = fat32_extract_lfn_char(lfn_entry, j);
                        if (c == '\0') break;
                        lfn_buffer[(entry_order - 1) * 13 + j] = c;
                    }
                    lfn_order = entry_order;
                }

                if (lfn_entry->order & 0x40) {
                    lfn_length = (entry_order - 1) * 13 + 13;
                }

                continue;
            }

            vfs_dir_entry_t *vfs_entry = &entries[entry_count];
            if (lfn_length > 0) {
                // lfn_buffer[lfn_length] = '\0';
                strncpy(vfs_entry->name, lfn_buffer, sizeof(vfs_entry->name) - 1);
                memset(lfn_buffer, 0, sizeof(lfn_buffer));
                lfn_length = 0;
                lfn_order = -1;
            } else {
                // Use 8.3 file name
                strncpy(vfs_entry->name, (char *)entry->filename, 8);
                vfs_entry->name[8] = '\0'; // Ensure null-termination
                char *ext = vfs_entry->name + strlen(vfs_entry->name);
                if (entry->ext[0] != ' ') {
                    if (strlen(vfs_entry->name) < sizeof(vfs_entry->name) - 1) {
                        strncat(vfs_entry->name, ".", sizeof(vfs_entry->name) - strlen(vfs_entry->name) - 1);
                        strncat(vfs_entry->name, (char *)entry->ext, 3);
                    }
                }
            }

            vfs_entry->type = (entry->attributes & FAT32_ATTR_DIRECTORY) ? VFS_MODE_DIR : VFS_MODE_FILE;
            vfs_entry->inode_number = fat32_entry_to_inode_number(entry, i);

            if (++entry_count >= max_entries) return entry_count;
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    return entry_count;
}

static int fat32_readdir_2(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry) {
    if (!dir || !entry) return -1;

    fat32_inode_t *dir_inode = (fat32_inode_t*)dir->fs_data;
    fat32_superblock_t *sb = (fat32_superblock_t*)dir->superblock->fs_data;

    uint32_t cluster = dir_inode->cluster;
    uint8_t buffer[sb->cluster_size];
    uint32_t current_offset = 0;

    char lfn_buffer[256] = {0}; // Buffer to store LFN
    size_t lfn_length = 0;
    int lfn_order = -1;

    while (cluster < FAT32_CLUSTER_LAST) {
        if (fat32_read_cluster(sb, cluster, buffer) != 0) return -1;

        for (int i = 0; i < sb->cluster_size / sizeof(fat32_dir_entry_t); i++, current_offset++) {
            fat32_dir_entry_t *dir_entry = (fat32_dir_entry_t*)(buffer + i * sizeof(fat32_dir_entry_t));

            if (dir_entry->filename[0] == FAT32_LAST_ENTRY) return 0;  // No more entries
            if (dir_entry->filename[0] == FAT32_DELETED_ENTRY) continue;

            if (current_offset < offset) continue;

            if (dir_entry->attributes == FAT32_ATTR_LONG_NAME) {
                fat32_long_dir_entry_t *lfn_entry = (fat32_long_dir_entry_t*)dir_entry;
                int entry_order = lfn_entry->order & 0x1F;

                if (lfn_order == -1 || entry_order == lfn_order - 1) {
                    // Append characters in correct order
                    for (int j = 0; j < 13; j++) {
                        char c = fat32_extract_lfn_char(lfn_entry, j);
                        if (c == '\0') break;
                        lfn_buffer[(entry_order - 1) * 13 + j] = c;
                    }
                    lfn_order = entry_order;
                }

                if (lfn_entry->order & 0x40) {
                    lfn_length = (entry_order - 1) * 13 + 13;
                }

                continue;
            }

            // Populate VFS entry
            if (lfn_length > 0) {
                strncpy(entry->name, lfn_buffer, sizeof(entry->name) - 1);
                memset(lfn_buffer, 0, sizeof(lfn_buffer));
                lfn_length = 0;
                lfn_order = -1;
            } else {
                // Use 8.3 filename
                strncpy(entry->name, (char *)dir_entry->filename, 8);
                entry->name[8] = '\0'; // Ensure null-termination
                char *ext = entry->name + strlen(entry->name);
                if (dir_entry->ext[0] != ' ') {
                    if (strlen(entry->name) < sizeof(entry->name) - 1) {
                        strncat(entry->name, ".", sizeof(entry->name) - strlen(entry->name) - 1);
                        strncat(entry->name, (char *)dir_entry->ext, 3);
                    }
                }
            }

            entry->type = (dir_entry->attributes & FAT32_ATTR_DIRECTORY) ? VFS_MODE_DIR : VFS_MODE_FILE;
            entry->inode_number = fat32_entry_to_inode_number(dir_entry, i);
            return current_offset + 1; // Return next offset
        }

        cluster = fat32_get_next_cluster(sb, cluster);
    }

    return 0; // End of directory
}

static int fat32_mkdir(vfs_inode_t *dir, const char* name, uint32_t mode) {
    return fat32_create(dir, name, mode | VFS_MODE_DIR);
}

static int fat32_rmdir(vfs_inode_t *dir, const char* name) {
    return fat32_unlink(dir, name);
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
    sb->data_sector = boot_sector->reserved_sectors + (boot_sector->fat_count * boot_sector->sectors_per_fat_large);
    sb->root_cluster = boot_sector->root_cluster;
    sb->data_clusters = (boot_sector->total_sectors_large - sb->data_sector) / boot_sector->sectors_per_cluster;
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