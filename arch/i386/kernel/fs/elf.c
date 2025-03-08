#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/process.h"
#include "libc/string.h"
#include "kernel/printf.h"

int is_valid_elf(elf_header_t *header) {
    if (header->magic != ELF_MAGIC) return 0;
    return 1;
}

process_t* load_elf(const char *path) {
    int fd = vfs_open(path, VFS_FLAG_READ);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return NULL;
    }

    elf_header_t header;
    if (vfs_read(fd, &header, sizeof(elf_header_t)) != sizeof(elf_header_t)) {
        printf("Error: Failed to read ELF header\n");
        vfs_close(fd);
        return NULL;
    }

    if (!is_valid_elf(&header)) {
        printf("Error: Invalid ELF file\n");
        vfs_close(fd);
        return NULL;
    }

    // Load program headers
    elf_program_header_t ph;

    for (int i = 0; i < header.ph_entry_count; i++) {
        vfs_seek(fd, header.ph_offset + i * sizeof(elf_program_header_t), VFS_SEEK_SET);
        if (vfs_read(fd, &ph, sizeof(elf_program_header_t)) != sizeof(elf_program_header_t)) {
            printf("Error: Failed to read program header\n");
            vfs_close(fd);
            return NULL;
        }

        // printf("Loading segment %d (type %d) | offset: %x | size: %x\n", i, ph.type, ph.offset, ph.file_size);
        if (ph.type != PT_LOAD) continue;

        // void *buf = kmalloc(ph.mem_size);
        // printf("  Virtual Address: %x\n", ph.vaddr);
        // printf("  Physical Address: %x\n", ph.paddr);
        // printf("  File Offset: %x\n", ph.offset);
        // printf("  File Size: %x\n", ph.file_size);
        // printf("  Memory Size: %x\n", ph.mem_size);
        // printf("  Flags: %x\n", ph.flags);
        // printf("  Align: %x\n", ph.align);

        kmap_memory(ph.vaddr, 0, ph.mem_size, 0x7);
        void *buf = (void *)ph.vaddr;

        vfs_seek(fd, ph.offset, VFS_SEEK_SET);
        if (vfs_read(fd, buf, ph.file_size) != ph.file_size) {
            // printf("Error: Failed to read program header\n");
            kfree(buf);
            vfs_close(fd);
            return NULL;
        }

        memset((uint8_t*)(buf + ph.file_size), 0, ph.mem_size - ph.file_size);
    }

    // printf("Entry point: %x\n", header.entry);
    process_t* proc = create_process(path, (void*)header.entry);
    vfs_close(fd);

    if (proc == NULL) {
        printf("Error: Failed to create process\n");
        return NULL;
    }

    return proc;
}