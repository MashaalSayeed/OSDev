#include "kernel/pmm.h"
#include "kernel/multiboot.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "system.h"
#include <stdbool.h>
 
uint8_t *bitmap = (uint8_t *)(& _kernel_end);
uint8_t *mem_start;
uint32_t total_blocks;
uint32_t bitmap_size;

bool out_of_memory = false;


void pmm_init(struct multiboot_info *mbd, uint32_t mem_size) {
    total_blocks = mem_size / BLOCK_SIZE;
    bitmap_size = CEILDIV(total_blocks, BLOCKS_PER_BUCKET);

    // Clear the bitmap
    memset(bitmap, 0, bitmap_size);
    
    mem_start = (uint8_t*)BLOCK_ALIGN((uint32_t) bitmap + bitmap_size);
    UNUSED(mbd);
    // print_memory_map(mbd);
    // printf("mem size:     %d mb\n", mem_size / (1024 * 1024));
    // printf("total_blocks: %d\n", total_blocks);
    // printf("bitmap addr:  %x\n", bitmap);
    // printf("bitmap_size:  %d\n", bitmap_size);
    // printf("mem_start:    %x\n", mem_start);
}

uint32_t allocate_block() {
    uint32_t block = first_free_block();
    if (out_of_memory == true) {
        printf("Error: Out of memory\n");
        return 0;
    }

    SETBIT(block);
    return block;
}

uint32_t first_free_block() {
    out_of_memory = false;
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!ISSET(i)) {
            return i;
        }
    }
    out_of_memory = true;
    return 0;
}

void free_block(uint32_t block) {
    CLEARBIT(block);
}

void print_memory_map(struct multiboot_info *mbd) {
    for (uint32_t i=0; i < mbd->mmap_length; i += sizeof(struct multiboot_mmap_entry)) {
        struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry*) (mbd->mmap_addr + i);
        printf("Low Addr: %x | High Addr: %x | Low Len: %x | High Len: %x | Size: %d | Type: %d\n", 
            entry->addr_low, entry->addr_high, entry->len_low, entry->len_high, entry->size, entry->type);
    }
}