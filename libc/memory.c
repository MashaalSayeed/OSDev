#include "libc/memory.h"
#include "kernel/memory.h"

static uint32_t heap_start;
static uint32_t heap_size;
static uint32_t threshold;

void kmalloc_init(uint32_t initial_heap_size) {
    heap_start = KERNEL_MALLOC;
    heap_size = 0;
    threshold = 0;

    change_heap_size(initial_heap_size);
    *(uint32_t*)heap_start = 0;
}

void change_heap_size(uint32_t new_heap_size) {
    int old_page_top = CEIL_DIV(heap_size, PAGE_SIZE);
    int new_page_top = CEIL_DIV(new_heap_size, PAGE_SIZE);

    int diff = new_page_top - old_page_top;

    for (int i = 0; i < diff; i++) {
        void *page = allocate_page();
        if (page == 0) {
            // Out of memory
            return;
        }

        memmap_page(KERNEL_MALLOC + old_page_top * 0x1000 + i * 0x1000, (uint32_t)page, PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
    }

    heap_size = new_heap_size;
}

void* kmalloc(uint32_t size) {
    size = (size + 3) & ~3; // Align to 4 bytes
    if (size + sizeof(uint32_t) > heap_size - threshold) {
        // Request for more memory
        return 0;
    }

    // Allocate memory
    uint32_t *ptr = (uint32_t*)(heap_start + threshold);
    threshold += size;
    return ptr;
}

void kfree(void *ptr, uint32_t size) {

}

void memset(void *dest, char val, uint32_t count){
    char *temp = (char*) dest;
    for (; count != 0; count --){
        *temp++ = val;
    }
}