#pragma once

#include <stdint.h>
#include "kernel/multiboot.h"

#define BLOCK_SIZE 4096
#define BLOCKS_PER_BUCKET 8

#define CEILDIV(a, b) ((a + b - 1) / b)

#define SETBIT(i) bitmap[i / BLOCKS_PER_BUCKET] = bitmap[i / BLOCKS_PER_BUCKET] | (1 << (i % BLOCKS_PER_BUCKET))
#define CLEARBIT(i) bitmap[i / BLOCKS_PER_BUCKET] = bitmap[i / BLOCKS_PER_BUCKET] & (~(1 << (i % BLOCKS_PER_BUCKET)))
#define ISSET(i) ((bitmap[i / BLOCKS_PER_BUCKET] >> (i % BLOCKS_PER_BUCKET)) & 0x1)
#define GET_BUCKET32(i) (*((uint32_t*) &bitmap[i / 32]))

#define BLOCK_ALIGN(addr) (((addr) & 0xFFFFF000) + 0x1000)

extern const uint32_t _kernel_end;

void pmm_init(struct multiboot_tag *mbd, uint32_t mem_size);
uint32_t pmm_alloc_block();
static uint32_t first_free_block();
void pmm_free_block(uint32_t block);