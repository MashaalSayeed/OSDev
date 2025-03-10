#pragma once

#include <stdint.h>
#include "multiboot.h"

#define ELF_MAGIC 0x464C457F

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xF)

typedef struct {
    uint32_t magic;
    uint8_t  elf[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t ph_offset;
    uint32_t sh_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t ph_entry_size;
    uint16_t ph_entry_count;
    uint16_t sh_entry_size;
    uint16_t sh_entry_count;
    uint16_t sh_str_index;
} __attribute__((packed)) elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t file_size;
    uint32_t mem_size;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) elf_program_header_t;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} __attribute__((packed)) elf_section_header_t;

typedef struct {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
} __attribute__((packed)) elf_symbol_t;

typedef struct {
    elf_symbol_t *symtab;
    uint32_t symtabsz;
    const char *strtab;
    uint32_t strtabsz;
} elf_t;

int is_valid_elf(elf_header_t *header);
elf_header_t* load_elf(const char *path);
elf_t elf_from_multiboot(struct multiboot_tag_elf_sections *elf_sec);
const char *elf_lookup_symbol(uint32_t addr, elf_t *elf);