#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/string.h"
#include "kernel/printf.h"
#include "kernel/multiboot.h"

extern int paging_enabled;

int is_valid_elf(elf_header_t *header) {
    if (header->magic != ELF_MAGIC) return 0;
    return 1;
}

elf_header_t* load_elf(const char *path) {
    int fd = vfs_open(path, VFS_FLAG_READ);
    if (fd < 0) return NULL;

    elf_header_t * header = kmalloc(sizeof(elf_header_t));
    if (vfs_read(fd, header, sizeof(elf_header_t)) != sizeof(elf_header_t)) {
        printf("Error: Failed to read ELF header\n");
        vfs_close(fd);
        return NULL;
    }

    if (!is_valid_elf(header)) {
        printf("Error: Invalid ELF file\n");
        vfs_close(fd);
        return NULL;
    }

    // Load program headers
    elf_program_header_t ph;

    for (int i = 0; i < header->ph_entry_count; i++) {
        vfs_seek(fd, header->ph_offset + i * sizeof(elf_program_header_t), VFS_SEEK_SET);
        if (vfs_read(fd, &ph, sizeof(elf_program_header_t)) != sizeof(elf_program_header_t)) {
            printf("Error: Failed to read program header\n");
            vfs_close(fd);
            return NULL;
        }

        if (ph.type != PT_LOAD) continue;
        kmap_memory(ph.vaddr, 0, ph.mem_size, 0x7);
        void *buf = (void *)ph.vaddr;

        vfs_seek(fd, ph.offset, VFS_SEEK_SET);
        if (vfs_read(fd, buf, ph.file_size) != ph.file_size) {
            printf("Error: Failed to read program header\n");
            kfree(buf);
            vfs_close(fd);
            return NULL;
        }

        memset((uint8_t*)(buf + ph.file_size), 0, ph.mem_size - ph.file_size);
    }

    vfs_close(fd);
    return header;
}

elf_t elf_from_multiboot(struct multiboot_tag_elf_sections * elf_sec) {
    elf_t elf;
    elf_section_header_t* sh = (elf_section_header_t *)elf_sec->sections;
    uint32_t shstrtab = sh[elf_sec->shndx].addr;

    for (int i = 0; i < elf_sec->num; i++) {
        char *name = (char *)(shstrtab + sh[i].name);
        if (strcmp(name, ".strtab") == 0) {
            elf.strtab = (const char *)sh[i].addr;
            elf.strtabsz = sh[i].size;
        }
        if (strcmp(name, ".symtab") == 0) {
            elf.symtab = (elf_symbol_t *)sh[i].addr;
            elf.symtabsz = sh[i].size;
        }
    }

    return elf;
}

const char *elf_lookup_symbol(uint32_t addr, elf_t *elf) {
    if (!paging_enabled || !elf) return NULL;
    for (int i = 0; i < (elf->symtabsz / sizeof(elf_symbol_t)); i++) {
        if (ELF32_ST_TYPE(elf->symtab[i].info) != 0x2) {
            continue;
        }

        if ((addr >= elf->symtab[i].value) && (addr < (elf->symtab[i].value + elf->symtab[i].size))) {
            return (const char *)((uint32_t)elf->strtab + elf->symtab[i].name);
        }
    }

    return NULL;
}