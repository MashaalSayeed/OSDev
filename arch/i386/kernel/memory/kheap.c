#include "kernel/kheap.h"

void *kheap_start;
void *kheap_end;
void *kheap_curr;


void kheap_init() {
    // Allocate 48MB of memory for the kernel heap
    // kheap_start = (uint8_t*)BLOCK_ALIGN((uint32_t) mem_start + bitmap_size);
    kheap_start = KHEAP_START;
    kheap_end = kheap_start + KHEAP_INITIAL_SIZE;
    kheap_curr = kheap_start;
}