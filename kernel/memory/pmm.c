#include "kernel/pmm.h"
#include "kernel/multiboot.h"
#include "libc/string.h"
#include "kernel/printf.h"
#include "kernel/system.h"
#include "kernel/locks.h"
#include <stdbool.h>
 
uint8_t *bitmap = (uint8_t *)(& _kernel_end);
uint8_t *mem_start;
uint32_t total_blocks;
uint32_t bitmap_size;

static spinlock_t pmm_lock;
bool out_of_memory = false;


void pmm_mark_used(uint32_t base, uint32_t length) {
    uint32_t start_block = base / BLOCK_SIZE;
    uint32_t end_block = (base + length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (end_block > total_blocks) {
        printf("Warning: Memory region exceeds total memory size, truncating\n");
        end_block = total_blocks;
    }

    for (uint32_t i = start_block; i < end_block; i++) {
        SETBIT(i);
    }
}

static void pmm_mark_free(uint32_t base, uint32_t length) {
    uint32_t start_block = base / BLOCK_SIZE;
    uint32_t end_block = (base + length + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (end_block > total_blocks) {
        printf("Warning: Memory region exceeds total memory size, truncating\n");
        end_block = total_blocks;
    }

    for (uint32_t i = start_block; i < end_block; i++) {
        CLEARBIT(i);
    }
}

void pmm_init(struct multiboot_tag *mbd, uint32_t mem_size) {
    total_blocks = mem_size / BLOCK_SIZE;
    bitmap_size = CEILDIV(total_blocks, BLOCKS_PER_BUCKET);

    // Clear the bitmap
    memset(bitmap, 0xFF, bitmap_size);
    
    mem_start = (uint8_t*)BLOCK_ALIGN((uint32_t) bitmap + bitmap_size);
    // Parse the multiboot memory map and mark available regions as free
    struct multiboot_tag *tag = mbd;
    for (; tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7))) 
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)tag;
            struct multiboot_mmap_entry *entry = mmap_tag->entries;

            while ((uint32_t)entry < (uint32_t)mmap_tag + mmap_tag->size) {
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    pmm_mark_free(entry->addr, entry->len);
                }

                entry = (struct multiboot_mmap_entry *)((uint32_t)entry + mmap_tag->entry_size);
            }
        }
    }

    // Reserve the entire low 1MB unconditionally.
    // This covers: IVT (0x0), BIOS data, VGA buffer (0xB8000), BIOS ROM.
    pmm_mark_used(0x0, 0x100000);

    // After the multiboot parsing loop and kernel region marking:
    pmm_mark_used(0xA0000, 0x60000);

    // Mark the memory used by the kernel + bitmap region as used
    uint32_t kernel_phys_start = (uint32_t)&_kernel_start;
    uint32_t kernel_phys_end = (uint32_t)bitmap + bitmap_size - LOAD_MEMORY_ADDRESS;
    pmm_mark_used(kernel_phys_start, kernel_phys_end - kernel_phys_start);
    printf("Kernel memory range: %x - %x\n", kernel_phys_start, kernel_phys_end);

    spinlock_init(&pmm_lock);

    uint32_t free_blocks = 0;
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!ISSET(i)) {
            free_blocks++;
        }
    }
    kprintf(INFO, "PMM initialized: %d/%d free blocks (%d MB)\n", free_blocks, total_blocks, (free_blocks * BLOCK_SIZE) / (1024 * 1024));
}

static uint32_t first_free_block() {
    out_of_memory = false;
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!ISSET(i)) {
            return i;
        }
    }
    out_of_memory = true;
    return 0;
}

uint32_t pmm_alloc_block() {
    uint32_t flags;
    spinlock_acquire_irq(&pmm_lock, &flags);

    uint32_t block = first_free_block();
    if (out_of_memory == true) {
        printf("Error: Out of memory\n");
        spinlock_release_irq(&pmm_lock, flags);
        return 0;
    }

    SETBIT(block);
    spinlock_release_irq(&pmm_lock, flags);
    return block;
}

void pmm_free_block(uint32_t block) {
    uint32_t flags;
    spinlock_acquire_irq(&pmm_lock, &flags);
    if (block == 0) {
        printf("Warning: Attempt to free block 0, ignoring\n");
        spinlock_release_irq(&pmm_lock, flags);
        return;
    }

    CLEARBIT(block);
    spinlock_release_irq(&pmm_lock, flags);
}