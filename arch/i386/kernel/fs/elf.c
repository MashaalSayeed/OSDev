#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/string.h"

extern page_directory_t *kpage_dir;

int is_valid_elf(elf_header_t *header) {
    if (header->magic != ELF_MAGIC) return 0;
    return 1;
}

void load_elf(const char *path) {
    int fd = vfs_open(path, VFS_FLAG_READ);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return;
    }

    elf_header_t header;
    if (vfs_read(fd, &header, sizeof(elf_header_t)) != sizeof(elf_header_t)) {
        printf("Error: Failed to read ELF header\n");
        vfs_close(fd);
        return;
    }

    if (!is_valid_elf(&header)) {
        printf("Error: Invalid ELF file\n");
        vfs_close(fd);
        return;
    }

    printf("Loading ELF file\n");
    printf("Entry point: %x\n", header.entry);

    // Load program headers
    elf_program_header_t ph;
    vfs_seek(fd, header.ph_offset, VFS_SEEK_SET);

    for (int i = 0; i < header.ph_entry_count; i++) {
        if (vfs_read(fd, &ph, sizeof(elf_program_header_t)) != sizeof(elf_program_header_t)) {
            printf("Error: Failed to read program header\n");
            vfs_close(fd);
            return;
        }

        if (ph.type != PT_LOAD) continue;

        // void *buf = kmalloc(ph.mem_size);
        map_pages(kpage_dir, ph.vaddr, ph.mem_size, 0x3);
        void *buf = (void *)ph.vaddr;

        vfs_seek(fd, ph.offset, VFS_SEEK_SET);
        if (vfs_read(fd, buf, ph.file_size) != ph.file_size) {
            printf("Error: Failed to read program header\n");
            kfree(buf);
            vfs_close(fd);
            return;
        }

        memset((uint8_t*)(buf + ph.file_size), 0, ph.mem_size - ph.file_size);
    }

    printf("Entry: %x\n", header.entry);

    void (*entry)() = (void(*)())header.entry;
    entry();
    vfs_close(fd);
}