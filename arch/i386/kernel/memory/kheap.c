#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "kernel/process.h"

#define MIN_BLOCK_SIZE (sizeof(kheap_block_t) + KHEAP_ALIGNMENT)

uint8_t *kheap_start;
uint8_t *kheap_end;
uint8_t *kheap_curr;

static kheap_block_t *free_list;

void *kmalloc(size_t size) {
    if (size < KHEAP_ALIGNMENT) {
        size = KHEAP_ALIGNMENT;
    }

    size = ALIGN_UP(size, KHEAP_ALIGNMENT);

    kheap_block_t *cur_block = free_list;
    kheap_block_t *prev_block = NULL;

    while (cur_block) {
        if (cur_block->size >= size) {
            if (cur_block->size >= size + MIN_BLOCK_SIZE) {
                // Split only if the remaining part is big enough for another block
                kheap_block_t *new_block = (kheap_block_t *)((uint8_t *)cur_block + sizeof(kheap_block_t) + size);
                new_block->size = cur_block->size - size - sizeof(kheap_block_t);
                new_block->next = cur_block->next;

                cur_block->size = size;

                if (prev_block) {
                    prev_block->next = new_block;
                } else {
                    free_list = new_block;
                }
            } else {
                // Use the whole block
                if (prev_block) {
                    prev_block->next = cur_block->next;
                } else {
                    free_list = cur_block->next;
                }
            }

            return (void *)((uint8_t *)cur_block + sizeof(kheap_block_t));
        }

        prev_block = cur_block;
        cur_block = cur_block->next;
    }

    return NULL; // Out of memory
}

void kfree(void *ptr) {
    if (!ptr) return;

    kheap_block_t *block = (kheap_block_t *)((uint8_t *)ptr - sizeof(kheap_block_t));

    // Insert back into the free list in sorted order
    kheap_block_t *cur_block = free_list;
    kheap_block_t *prev_block = NULL;

    while (cur_block && cur_block < block) {
        prev_block = cur_block;
        cur_block = cur_block->next;
    }

    block->next = cur_block;
    if (prev_block) {
        prev_block->next = block;
    } else {
        free_list = block;
    }

    // Coalesce adjacent blocks
    cur_block = free_list;
    while (cur_block && cur_block->next) {
        kheap_block_t *next_block = cur_block->next;

        if ((uint8_t *)cur_block + cur_block->size + sizeof(kheap_block_t) == (uint8_t *)next_block) {
            // Combine adjacent blocks
            cur_block->size += next_block->size + sizeof(kheap_block_t);
            cur_block->next = next_block->next;
        } else {
            cur_block = next_block;
        }
    }
}

void *kmalloc_aligned(size_t size, size_t align) {
    uintptr_t raw_mem = (uintptr_t)kmalloc(size + align - 1 + sizeof(void*));
    if (!raw_mem) return NULL;

    // Store the original pointer just before the aligned pointer
    uintptr_t aligned = ALIGN_UP(raw_mem + sizeof(void*), align);
    ((void**)aligned)[-1] = (void*)raw_mem;

    return (void*)aligned;
}

void kfree_aligned(void *ptr) {
    if (ptr) {
        // Get the original pointer from just before the aligned pointer
        void *raw_ptr = ((void**)ptr)[-1];
        kfree(raw_ptr);
    }
}

void print_kheap() {
    kheap_block_t *cur_block = free_list;
    uint32_t total_free = 0;
    while (cur_block) {
        printf("Free Blocks: %x, Size: %x\n", cur_block, cur_block->size);
        total_free += cur_block->size;
        cur_block = cur_block->next;
    }
    printf("Total Free: %x\n", total_free);
}

void *calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = kmalloc(total_size);
    
    if (ptr) {
        // Initialize allocated memory to 0
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total_size; i++) {
            p[i] = 0;
        }
    }
    
    return ptr;
}

void kheap_init() {
    // Allocate 48MB of memory for the kernel heap
    kheap_start = (uint8_t *)KHEAP_START;
    kheap_end = kheap_start + KHEAP_INITIAL_SIZE;
    kheap_curr = kheap_start;

    free_list = (kheap_block_t *)kheap_start;
    free_list->size = KHEAP_INITIAL_SIZE - sizeof(kheap_block_t);
    free_list->next = NULL;
}