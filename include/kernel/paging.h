#pragma once

#include <stdint.h>
#include "kernel/isr.h"

#define ERR_PRESENT     0x1
#define ERR_RW          0x2
#define ERR_USER        0x4
#define ERR_RESERVED    0x8
#define ERR_INST        0x10

#define IS_ALIGN(addr) ((((uint32_t)(addr)) | 0xFFFFF000) == 0)
#define PAGE_ALIGN(addr) ((((uint32_t)(addr)) & 0xFFFFF000) + 0x1000)

typedef struct page_directory_entry {
    unsigned int present    : 1;
    unsigned int rw         : 1;
    unsigned int user       : 1;
    unsigned int accessed   : 1;
    unsigned int reserved   : 1;
    unsigned int unused     : 7;
    unsigned int frame      : 20;
} page_directory_entry_t;

typedef struct page_table_entry {
    unsigned int present    : 1;
    unsigned int rw         : 1;
    unsigned int user       : 1;
    unsigned int accessed   : 1;
    unsigned int dirty      : 1;
    unsigned int unused     : 7;
    unsigned int frame      : 20;
} page_table_entry_t;

typedef struct page_table {
    page_table_entry_t pages[1024];
} page_table_t;

typedef struct page_directory {
    page_directory_entry_t tables[1024]; // Array of page directory entries
    page_table_t *ref_tables[1024];      // Array of pointers to the virtual address page tables
} page_directory_t;


extern page_directory_t * initial_page_dir;
extern uint32_t _kernel_start;

void paging_init();
void switch_page_directory(page_directory_t *dir, uint32_t phys);
void enable_paging();
void allocate_page(page_directory_t *dir, uint32_t virtual, uint32_t flags);
void free_page(page_directory_t *dir, uint32_t virtual);
void * physical2virtual(page_directory_t *dir, void *physical);
void * virtual2physical(page_directory_t *dir, void *virtual);
void map_physical_to_virtual(uint32_t virtual, uint32_t physical);
void page_fault_handler(registers_t *regs);