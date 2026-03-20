#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kernel/isr.h"

#define UACCESS_TMP_DST  0xD0000000   // temp window for writes to user space
#define UACCESS_TMP_SRC  0xD0001000   // temp window for reads from user space

#define PAGE_SIZE       0x1000
#define PAGE_ENTRIES     1024
#define USER_HEAP_START 0x400000

#define PAGE_PRESENT     0x1
#define PAGE_RW          0x2
#define PAGE_USER        0x4
#define PAGE_SIZE_4MB    0x80

#define PF_ERR_PRESENT     0x1
#define PF_ERR_RW          0x2
#define PF_ERR_USER        0x4
#define PF_ERR_RESERVED    0x8
#define PF_ERR_INST        0x10

#define IS_ALIGN(addr) (((uint32_t)(addr) & (PAGE_SIZE - 1)) == 0)
#define PAGE_ALIGN(addr) ((uint32_t)(addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define ASSERT(cond) do { \
    if (!(cond)) { \
        kprintf(ERROR, "ASSERT FAILED: %s in %s(), at %s:%d\n", #cond, __func__, __FILE__, __LINE__); \
        asm volatile ("cli; hlt"); \
    } \
} while (0)

typedef struct page_directory_entry {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t accessed   : 1;
    uint32_t reserved   : 1;
    uint32_t unused     : 7;
    uint32_t frame      : 20;
} __attribute__((packed)) page_directory_entry_t;

typedef struct page_table_entry {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t unused     : 7;
    uint32_t frame      : 20;
} page_table_entry_t;

typedef struct page_table {
    page_table_entry_t pages[1024];
} __attribute__((packed)) page_table_t;

typedef struct page_directory {
    page_directory_entry_t tables[1024]; // Array of page directory entries
    page_table_t *ref_tables[1024];      // Array of pointers to the virtual address page tables
} __attribute__((packed)) page_directory_t;


extern page_directory_t * initial_page_dir;
extern const uint32_t _kernel_start;

void paging_init();
void switch_page_directory(page_directory_t *dir);
void enable_paging();
void invalidate_page(uint32_t virtual);
void get_cr3(uint32_t *cr3);

page_table_entry_t * get_page(uint32_t virtual, int make, page_directory_t *dir);
bool alloc_page(page_table_entry_t *page, uint32_t flags);
void free_page(page_table_entry_t *page);

void * virtual2physical(page_directory_t *dir, void *virtual);

// Copy from kernel buffer into user virtual address space
// Returns 0 on success, -1 on failure
int copy_to_user(page_directory_t *dir, uint32_t user_vaddr, const void *src, size_t size);
int copy_from_user(page_directory_t *dir, void *dst, uint32_t user_vaddr, size_t size);
int strncpy_from_user(page_directory_t *dir, char *dst, uint32_t user_vaddr, size_t max);

// Copy a physical page into a new physical frame — returns new frame number
// Used by fork's clone_page_directory
uint32_t copy_page_frame(page_directory_t *src_dir, uint32_t src_vaddr);

void map_page_to_frame(page_table_entry_t *page, uint32_t frame, uint32_t flags);
void map_memory(page_directory_t *dir, uint32_t virtual_start, uint32_t physical_start, uint32_t size, uint32_t flags);
void kmap_memory(uint32_t virtual_start, uint32_t physical_start, uint32_t size, uint32_t flags);

void debug_page_mapping(page_directory_t *dir, uint32_t virtual_address);
void dump_page_directory(page_directory_t *dir);
void page_fault_handler(registers_t *regs);
page_directory_t * clone_page_directory(page_directory_t *src);
void free_page_directory(page_directory_t *dir);