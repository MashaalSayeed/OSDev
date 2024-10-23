#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct kheap_block {
    size_t size;
    struct kheap_block *next;
} kheap_block_t;

uint8_t temp_heap[0x1000];

uint8_t * kheap_start;
uint8_t * kheap_end;
uint8_t * kheap_curr;
#define KHEAP_ALIGNMENT 8

static inline uint32_t align(uint32_t size) {
    return (size + (KHEAP_ALIGNMENT - 1)) & ~(KHEAP_ALIGNMENT - 1);
}

static kheap_block_t * free_list;

void * kmalloc (size_t size) {
    size = align(size);
    kheap_block_t *cur_block = free_list;
    kheap_block_t *prev_block = NULL;

    while (cur_block) {
        // Find the first block that is large enough
        if (cur_block->size >= size) {
            if (cur_block->size > size + sizeof(kheap_block_t)) {
                // Split the block (Used + Free)
                kheap_block_t *new_block = (kheap_block_t *)((uint8_t *)cur_block + sizeof(kheap_block_t) + size);
                new_block->size = cur_block->size - size - sizeof(kheap_block_t);
                new_block->next = cur_block->next;

                cur_block->size = size;
                cur_block->next = NULL;

                if (prev_block) {
                    prev_block->next = new_block;
                } else {
                    free_list = new_block;
                }
            } else {
                // Remove the block from the free list
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

    return NULL;
}

void kfree (void * ptr) {
    if (!ptr) return;

    kheap_block_t *block = (kheap_block_t *)(ptr - sizeof(kheap_block_t));
    block->next = free_list; // Add the block to the free list
    free_list = block;

    // Coalesce free blocks
    kheap_block_t *cur_block = free_list;
    while (cur_block) {
        kheap_block_t *next_block = cur_block->next;
        // If the current block and the next block are contiguous
        if (next_block && (uint32_t)cur_block + cur_block->size + sizeof(kheap_block_t) == (uint32_t)next_block) {
            cur_block->size += next_block->size + sizeof(kheap_block_t);
            cur_block->next = next_block->next;
        } else {
            cur_block = next_block;
        }
    }
}

void print_kheap () {
    return;
}

void kheap_init () {
    kheap_start = temp_heap;
    kheap_end = temp_heap + sizeof(temp_heap);
    kheap_curr = kheap_start;

    free_list = (kheap_block_t *) kheap_start;
    free_list->size = sizeof(temp_heap) - sizeof(kheap_block_t);
    free_list->next = NULL;
}

void main() {
    kheap_init();
    uint8_t * ptr1 = kmalloc(0x20);
    uint8_t * ptr2 = kmalloc(0x20);

    printf("Allocated memory at %x\n", ptr1);
    printf("Allocated memory at %x\n", ptr2);

    kfree(ptr1);
    printf("Freed memory at %x\n", ptr1);
    return;
}