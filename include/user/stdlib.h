#pragma once

#include <stddef.h>

typedef struct block_header {
    size_t size;
    struct block_header *next;
    int free;
    char data[0]; // pointer to the start of the data area
} block_header_t;

void *malloc(size_t size);
void free(void *ptr);