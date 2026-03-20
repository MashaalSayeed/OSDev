#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/string.h"
#include "kernel/printf.h"
#include "kernel/multiboot.h"

extern int paging_enabled;
extern page_directory_t *kpage_dir;

int is_valid_elf(elf_header_t *header) {
    if (header->magic != ELF_MAGIC) return 0;
    return 1;
}

elf_header_t *load_elf(vfs_file_t *file, page_directory_t *page_dir) {
    elf_header_t *header = kmalloc(sizeof(elf_header_t));
    if (!header) return NULL;

    if (file->file_ops->read(file, header, sizeof(elf_header_t)) != sizeof(elf_header_t)) {
        kprintf(ERROR, "load_elf: failed to read ELF header\n");
        goto fail;
    }

    if (!is_valid_elf(header)) {
        kprintf(ERROR, "load_elf: invalid ELF magic\n");
        goto fail;
    }

    elf_program_header_t ph;
    for (int i = 0; i < header->ph_entry_count; i++) {
        file->file_ops->seek(file,
            header->ph_offset + i * sizeof(elf_program_header_t),
            VFS_SEEK_SET);

        if (file->file_ops->read(file, &ph, sizeof(elf_program_header_t))
                != sizeof(elf_program_header_t)) {
            kprintf(ERROR, "load_elf: failed to read program header %d\n", i);
            goto fail;
        }


        if (ph.type != PT_LOAD) continue;

        // Map physical frames into the new process page dir
        map_memory(page_dir, ph.vaddr, (uint32_t)-1, ph.mem_size, 0x7);
        page_table_entry_t *dbg = get_page(ph.vaddr, 0, page_dir);
        
        // Read segment data into kernel buffer
        uint8_t *tmp = kmalloc(ph.file_size);
        if (!tmp) {
            kprintf(ERROR, "load_elf: failed to allocate segment buffer\n");
            goto fail;
        }

        file->file_ops->seek(file, ph.offset, VFS_SEEK_SET);
        if (file->file_ops->read(file, tmp, ph.file_size) != ph.file_size) {
            kprintf(ERROR, "load_elf: failed to read segment %d\n", i);
            kfree(tmp);
            goto fail;
        }

        // Copy file data into user address space via copy_to_user
        if (copy_to_user(page_dir, ph.vaddr, tmp, ph.file_size) < 0) {
            kprintf(ERROR, "load_elf: copy_to_user failed for segment %d\n", i);
            kfree(tmp);
            goto fail;
        }
        kfree(tmp);

        // Zero BSS region (mem_size > file_size)
        if (ph.mem_size > ph.file_size) {
            uint32_t bss_vaddr = ph.vaddr + ph.file_size;
            uint32_t bss_size  = ph.mem_size - ph.file_size;
            uint8_t *zeros = kmalloc(bss_size);
            if (!zeros) {
                kprintf(ERROR, "load_elf: failed to allocate BSS zero buffer\n");
                goto fail;
            }
            memset(zeros, 0, bss_size);
            if (copy_to_user(page_dir, bss_vaddr, zeros, bss_size) < 0) {
                kprintf(ERROR, "load_elf: copy_to_user failed for BSS\n");
                kfree(zeros);
                goto fail;
            }
            kfree(zeros);
        }
    }

    return header;

fail:
    kfree(header);
    return NULL;
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

    // elf_symbol_t *symtab = (elf_symbol_t *)SYMTAB_VIRT_ADDR;
    // const char *strtab = (const char *)STRTAB_VIRT_ADDR;
    elf_symbol_t *symtab = elf->symtab;
    const char *strtab = elf->strtab;

    for (size_t i = 0; i < (elf->symtabsz / sizeof(elf_symbol_t)); i++) {
        if (ELF32_ST_TYPE(symtab[i].info) != 0x2) {
            continue;
        }

        if ((addr >= symtab[i].value) && (addr < (symtab[i].value + symtab[i].size))) {
            return (const char *)((uint32_t)strtab + symtab[i].name);
        }
    }

    return NULL;
}