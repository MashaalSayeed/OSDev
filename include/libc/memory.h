#pragma once

#include <stdint.h>

void kmalloc_init(uint32_t inital_heap_size);
void change_heap_size(uint32_t new_heap_size);

void* kmalloc(uint32_t size);
void memset(void *dest, char val, uint32_t count);