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


void pmm_init(struct multiboot_tag *mbd, uint32_t mem_size) {
    total_blocks = mem_size / BLOCK_SIZE;
    bitmap_size = CEILDIV(total_blocks, BLOCKS_PER_BUCKET);

    // Clear the bitmap
    memset(bitmap, 0, bitmap_size);
    
    mem_start = (uint8_t*)BLOCK_ALIGN((uint32_t) bitmap + bitmap_size);
    UNUSED(mbd);
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