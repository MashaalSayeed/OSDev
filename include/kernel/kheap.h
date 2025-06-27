#pragma once

#include <stdint.h>
#include <stddef.h>

// Kernel heap size = 48MB
#define KHEAP_START 0xC0400000
// #define KHEAP_INITIAL_SIZE 48 * 0x100000
#define KHEAP_INITIAL_SIZE 0x100000 // debug

#define KHEAP_MIN_SIZE 0x100000
#define KHEAP_ALIGNMENT 8

#define ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))

typedef struct kheap_block {
    size_t size;
    struct kheap_block *next;
} kheap_block_t;


// Aligns size to the nearest multiple of alignment
static inline uint32_t align(uint32_t size) {
    return (size + (KHEAP_ALIGNMENT - 1)) & ~(KHEAP_ALIGNMENT - 1);
}

void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t align);

void kfree(void *ptr);
void kfree_aligned(void *ptr);

void print_kheap();
void kheap_init();
void *calloc(size_t num, size_t size);