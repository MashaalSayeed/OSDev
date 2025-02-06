#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/process.h"


int is_valid_elf(elf_header_t *header) {
    if (header->magic != ELF_MAGIC) return 0;
    if (header->elf[0] != 0x7F || header->elf[1] != 'E' || header->elf[2] != 'L' || header->elf[3] != 'F') return 0;
    return 1;
}

void load_elf(void *elf_data) {
    elf_header_t *header = (elf_header_t*)elf_data;
    if (!is_valid_elf(header)) {
        printf("Error: Invalid ELF file\n");
        return;
    }

    printf("Loading ELF file\n");
    printf("Entry point: %x\n", header->entry);

    // elf_program_header_t *ph = (elf_program_header_t*)((uint32_t)elf_data + header->ph_offset);
    // for (int i = 0; i < header->ph_entry_count; i++) {
    //     if (ph[i].type == 1) {
    //         printf("Loading segment %d\n", i);
    //         printf("Vaddr: %x, Paddr: %x, File size: %x, Mem size: %x\n", ph[i].vaddr, ph[i].paddr, ph[i].file_size, ph[i].mem_size);
    //         memcpy((void*)ph[i].vaddr, (void*)((uint32_t)elf_data + ph[i].offset), ph[i].file_size);
    //         memset((void*)(ph[i].vaddr + ph[i].file_size), 0, ph[i].mem_size - ph[i].file_size);
    //     }
    // }

    // process_t *proc = create_process(header->entry);
    // if (proc == NULL) {
    //     printf("Error: Failed to create process\n");
    //     return;
    // }

    // printf("Process created: %d\n", proc->pid);
    // printf("Process entry: %x\n", proc->entry);
}