#pragma once

#include <stdint.h>
#include "multiboot.h"
#include "isr.h"

extern uint32_t initial_page_dir[1024];
extern int mem_num_vpages;

#define KERNEL_START 0xC0000000
#define KERNEL_MALLOC 0xC0100000


// 4KB pages
#define PAGE_SIZE 0x1000
// 4GB of memory
#define MEMORY_SIZE 0x100000000

#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_RW (1 << 1)
#define PAGE_FLAG_OWNER (1 << 9)

#define REC_PAGEDIR ((uint32_t*)0xFFFFF000)
#define REC_PAGETABLE(i) ((uint32_t*) (0xFFC00000 + ((i) << 12)))


// Error Codes
#define ERR_PRESENT     0x1
#define ERR_RW          0x2
#define ERR_USER        0x4
#define ERR_RESERVED    0x8
#define ERR_INST        0x10

#define CEIL_DIV(a, b) ((a + b - 1) / b)

void init_memory(struct multiboot_info *mbd);
void pmm_init(uint32_t mem_lower, uint32_t mem_upper);

void* allocate_page();
void free_page(void *ptr);

// Memory mapping for pages
void* mem_current_page_dir();
void mem_change_page_dir(uint32_t *page_dir);
void memmap_page(uint32_t virtual_addr, uint32_t phys_addr, uint32_t flags);

void print_memory_map(struct multiboot_info *mbd);
void page_fault_handler(registers_t *regs);
void invalidate_page(uint32_t addr);