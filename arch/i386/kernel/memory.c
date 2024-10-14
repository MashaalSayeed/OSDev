#include "kernel/memory.h"
#include "kernel/multiboot.h"
#include "libc/stdio.h"
#include "libc/string.h"

void init_memory(struct multiboot_info *mbd) {
    // uint32_t mem_lower = mbd->mem_lower;
    // uint32_t mem_upper = mbd->mem_upper;

    for (int i=0; i < mbd->mmap_length; i += sizeof(struct multiboot_mmap_entry)) {
        struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry*) (mbd->mmap_addr + i);
        printf("Low Addr: %x | High Addr: %x | Low Len: %x | High Len: %x | Size: %d | Type: %d\n", 
            entry->addr_low, entry->addr_high, entry->len_low, entry->len_high, entry->size, entry->type);
    }

    // printf("Memory Map Length: %d\n", mbd->mmap_length);
    // printf("Memory Map Address: %d\n", mbd->mmap_addr);
    // printf("Memory Lower: %d\n", mbd->mem_lower);
    // printf("Memory Upper: %d\n", mbd->mem_upper);
}