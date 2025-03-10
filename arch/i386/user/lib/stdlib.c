#include "user/stdlib.h"
#include "user/syscall.h"

#define ALIGN(size) (((size) + 7) & ~7)  // Align size to 8 bytes
#define BLOCK_SIZE sizeof(block_header_t)

static block_header_t *heap_start = NULL; // Start of the heap

void *malloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN(size);  // Ensure alignment

    block_header_t *current = heap_start, *prev = NULL;

    // Search for a free block
    while (current) {
        if (current->free && current->size >= size) {
            current->free = 0;
            return (void *)(current + 1);  // Return memory after the header
        }
        prev = current;
        current = current->next;
    }

    // No suitable block found; request more memory
    block_header_t *new_block = (block_header_t *)syscall_sbrk(size + BLOCK_SIZE);
    if (new_block == (void *)-1) return NULL;  // sbrk failed

    new_block->size = size;
    new_block->free = 0;
    new_block->next = NULL;

    if (prev) prev->next = new_block; // Append to list
    else heap_start = new_block; // First allocation

    return (void *)(new_block + 1);
}

void free(void *ptr) {
    if (!ptr) return;

    block_header_t *block = (block_header_t *)ptr - 1;
    block->free = 1;

    // Coalesce adjacent free blocks
    block_header_t *current = heap_start;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += BLOCK_SIZE + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}
