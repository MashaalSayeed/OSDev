#include "kernel/isr.h"
#include "kernel/paging.h"
#include "kernel/pmm.h"
#include "kernel/kheap.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "system.h"
#include "drivers/serial.h"


uint8_t * temp_mem;
page_directory_t *kpage_dir; // Kernel page directory (current)

extern uint8_t *bitmap;
extern uint32_t bitmap_size;

int paging_enabled = 0;

uint32_t dumb_kmalloc(uint32_t size, uint32_t align) {
    void *ret = temp_mem;
    if (align && !IS_ALIGN(ret)) {
        ret = (void*) PAGE_ALIGN(ret);
    }

    temp_mem = (uint8_t*) ret + size;
    return ret;
}

void * virtual2physical(page_directory_t * dir, void * virtual) {
    if (!paging_enabled) {
        return virtual - LOAD_MEMORY_ADDRESS;
    }

    uint32_t pd_index = (uint32_t)virtual >> 22;
    uint32_t pt_index = ((uint32_t)virtual >> 12) & 0x3FF;
    uint32_t page_frame_offset = (uint32_t)virtual & 0xFFF;

    page_table_t * table = dir->ref_tables[pd_index];
    if(!table) {
        printf("virtual2phys: page dir entry does not exist\n");
        return NULL;
    }

    page_table_entry_t pt_entry = table->pages[pt_index];
    if(!pt_entry.present) {
        printf("virtual2phys: page table entry does not exist\n");
        return NULL;
    }

    return (void *)(pt_entry.frame << 12) + page_frame_offset;
}

void *physical2virtual(page_directory_t *dir, void *physical) {
    if (!paging_enabled) {
        return physical + LOAD_MEMORY_ADDRESS;
    }

    uint32_t phys_addr = (uint32_t)physical;
    uint32_t pd_index = phys_addr >> 22; // Get page directory index
    uint32_t pt_index = (phys_addr >> 12) & 0x03FF; // Get page table index
    uint32_t page_frame_offset = phys_addr & 0xFFF; // Get offset within page

    page_table_t *table = dir->ref_tables[pd_index];
    if (!table) {
        printf("physical2virtual: page dir entry does not exist\n");
        return NULL;
    }

    page_table_entry_t pt_entry = table->pages[pt_index];
    if (!pt_entry.present) {
        printf("physical2virtual: page table entry does not exist\n");
        return NULL;
    }

    return (void *)((pd_index << 22) + (pt_index << 12) + page_frame_offset);
}

void allocate_page(page_directory_t *dir, uint32_t virtual, uint32_t flags) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    page_table_t *pt = dir->ref_tables[pd_index];
    if (!pt) {
        // Allocate a new page table
        pt = (page_table_t*) dumb_kmalloc(sizeof(page_table_t), 1);
        memset(pt, 0, sizeof(page_table_t));

        uint32_t t = (uint32_t) virtual2physical(kpage_dir, pt);
        dir->tables[pd_index].present = 1;
        dir->tables[pd_index].rw = 1;
        dir->tables[pd_index].user = 1;
        dir->tables[pd_index].frame = t >> 12;

        dir->ref_tables[pd_index] = pt;
    }

    if (!pt->pages[pt_index].present) {
        uint32_t frame = allocate_block();
        pt->pages[pt_index].present = 1;
        pt->pages[pt_index].rw = 1;
        pt->pages[pt_index].user = 1;
        pt->pages[pt_index].frame = frame;
    }
}

void free_page(page_directory_t *dir, uint32_t virtual) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    page_table_t *pt = dir->ref_tables[pd_index];
    if (!pt) {
        printf("free_page: page dir entry does not exist\n");
        return;
    }

    if (pt->pages[pt_index].present) {
        free_block(pt->pages[pt_index].frame);
        pt->pages[pt_index].present = 0;
    }
}

void map_physical_to_virtual(uint32_t virtual, uint32_t physical) {
    allocate_page(kpage_dir, virtual, 0x3);

    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    // Get the page table corresponding to the directory entry
    page_table_t *pt = kpage_dir->ref_tables[pd_index];
    if (pt) {
        pt->pages[pt_index].frame = physical >> 12; // Store physical frame
        pt->pages[pt_index].present = 1;  // Mark the page as present
        pt->pages[pt_index].rw = 1;       // Allow read/write access
        pt->pages[pt_index].user = 1;     // User accessible
    } else {
        // Handle case where page table was not allocated (error)
        printf("Error: Page table not found for virtual address %x\n", virtual);
    }
}

void switch_page_directory(page_directory_t *dir, uint32_t phys) {
    uint32_t t = (uint32_t) dir;
    if (!phys) {
        t = (uint32_t)virtual2physical(initial_page_dir, (void*)t);
    }

    asm volatile("mov %0, %%cr3" :: "r"(t));
}

void enable_paging() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    paging_enabled = 1;
}

void paging_init() {
    temp_mem = bitmap + bitmap_size;

    // Remove the temporary page directory in kernel's data section and install new one
    kpage_dir = (page_directory_t*) dumb_kmalloc(sizeof(page_directory_t), 1);
    memset(kpage_dir, 0, sizeof(page_directory_t));

    // Map the first 4MB of memory to the first 4MB of physical memory
    uint32_t i = LOAD_MEMORY_ADDRESS;
    while (i < LOAD_MEMORY_ADDRESS + 4 * 0x100000) {
        allocate_page(kpage_dir, i, 0x3);
        i += BLOCK_SIZE;
    }

    register_interrupt_handler(14, page_fault_handler);

    // Map some memory for the kernel heap
    while (i < LOAD_MEMORY_ADDRESS + 4 * 0x100000 + KHEAP_INITIAL_SIZE) {
        allocate_page(kpage_dir, i, 0x3);
        i += BLOCK_SIZE;
    }

    printf("Last page allocated: %x\n", i);

    // Map RSDP
    i = LOAD_MEMORY_ADDRESS + 0x000E0000;
    while (i < LOAD_MEMORY_ADDRESS + 0x00100000) {
        allocate_page(kpage_dir, i, 0x3);
        i += BLOCK_SIZE;
    }

    // Switch to the new page directory
    switch_page_directory(kpage_dir, 0);
    enable_paging();
}

void page_fault_handler(registers_t *regs) {
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

    log_to_serial("Page fault\n");

    printf("Page fault at %x\n", faulting_address);
    printf("Error code: %x\n", regs->err_code);

    uint32_t present = regs->err_code & ERR_PRESENT;
    uint32_t rw = regs->err_code & ERR_RW;
    uint32_t user = regs->err_code & ERR_USER;
    uint32_t reserved = regs->err_code & ERR_RESERVED;
    uint32_t inst_fetch = regs->err_code & ERR_INST;

    printf("Possible causes: [ ");
    if(!present) printf("Page not present ");
    if(rw) printf("Page is read only ");
    if(user) printf("Page is read only ");
    if(reserved) printf("Overwrote reserved bits ");
    if(inst_fetch) printf("Instruction fetch ");
    printf("]\n");

    for (;;) ;
}