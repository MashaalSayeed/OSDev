#include "user/stdlib.h"
#include "user/syscall.h"
#include "libc/string.h"

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
            return current->data;
        }
        prev = current;
        current = current->next;
    }

    // No suitable block found; request more memory
    block_header_t *new_block = (block_header_t *)syscall_sbrk(size + BLOCK_SIZE);
    if (new_block == (void *)-1) return NULL;

    new_block->size = size;
    new_block->free = 0;
    new_block->next = NULL;

    if (prev) prev->next = new_block; // Append to list
    else heap_start = new_block; // First allocation

    return new_block->data;
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;

    size_t total = nmemb * size;
    if (total / nmemb != size) return NULL;

    void *ptr = malloc(total);
    if (!ptr) return NULL;

    memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_header_t *block = (block_header_t *)((char *)ptr - BLOCK_SIZE);
    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    size_t copy_size = block->size < size ? block->size : size;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
    return new_ptr;
}

void free(void *ptr) {
    if (!ptr) return;

    block_header_t *block = (block_header_t *)((char *)ptr - BLOCK_SIZE);
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

static char tolower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = tolower_ascii(*s1);
        char c2 = tolower_ascii(*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }

    return tolower_ascii(*s1) - tolower_ascii(*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t count) {
    for (size_t i = 0; i < count; i++) {
        char c1 = tolower_ascii(s1[i]);
        char c2 = tolower_ascii(s2[i]);
        if (c1 != c2) {
            return c1 - c2;
        }
        if (s1[i] == '\0' || s2[i] == '\0') {
            return 0;
        }
    }

    return 0;
}
