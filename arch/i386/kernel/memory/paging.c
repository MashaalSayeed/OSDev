#include "kernel/isr.h"
#include "kernel/paging.h"
#include "kernel/pmm.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "system.h"
#include "drivers/serial.h"


uint8_t * temp_mem;
page_directory_t *kpage_dir; // Kernel page directory (current)

extern uint8_t *bitmap;
extern uint32_t bitmap_size;

int paging_enabled = 0;

void * dumb_kmalloc(uint32_t size, uint32_t align) {
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

    return (void *)((pt_entry.frame << 12) | page_frame_offset);
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

// Allocates a page in the page directory and page table for a given virtual address.
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
        dir->tables[pd_index].rw = (flags & 0x2) ? 1 : 0;
        dir->tables[pd_index].user = (flags & 0x4) ? 1 : 0;
        dir->tables[pd_index].frame = t >> 12;

        dir->ref_tables[pd_index] = pt;
    }

    if (!pt->pages[pt_index].present) {
        uint32_t frame = allocate_block();
        pt->pages[pt_index].present = 1;
        pt->pages[pt_index].rw = (flags & 0x2) ? 1 : 0;
        pt->pages[pt_index].user = (flags & 0x4) ? 1 : 0;
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

// Map a physical address to a virtual address by creating a new page table entry
void map_physical_to_virtual(uint32_t virtual, uint32_t physical) {
    if (!IS_ALIGN(physical)) {
        // printf("Error: Physical address is not page aligned %x to %x\n", physical, PAGE_ALIGN(physical));
        physical = PAGE_ALIGN(physical);
    }

    allocate_page(kpage_dir, virtual, 0x6);

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

void map_physical_to_virtual_region(uint32_t virtual_start, uint32_t physical_start, uint32_t size) {
    // Calculate the number of pages to map
    uint32_t num_pages = (size + 0xFFF) / PAGE_SIZE;
    printf("Mapping %d pages from %x to %x\n", num_pages, physical_start, virtual_start);
    for (uint32_t i = 0; i < num_pages; i++) {
        // Calculate the virtual and physical addresses for the current page
        uint32_t virtual_addr = virtual_start + (i * PAGE_SIZE);
        uint32_t physical_addr = physical_start + (i * PAGE_SIZE);

        // Map each page individually
        map_physical_to_virtual(virtual_addr, physical_addr);
    }
}

void switch_page_directory(page_directory_t *dir) {
    // Set the CR3 register to the physical address of the page directory
    if (!paging_enabled) {
        dir = (page_directory_t*) virtual2physical(kpage_dir, dir);
    } else {
        dir = (page_directory_t*) virtual2physical(kpage_dir, dir);
    }

    // printf("Switching page directory to %x\n", dir);
    asm volatile("mov %0, %%cr3" :: "r"(dir) : "memory");
}

page_directory_t * clone_page_directory(page_directory_t *src) {
    page_directory_t *new_dir = (page_directory_t*) dumb_kmalloc(sizeof(page_directory_t), 1);
    memset(new_dir, 0, sizeof(page_directory_t));
    int count = 0;

    // Copy kernel space
    for (uint32_t i = 0; i < 1024; i++) {
        if (!src->tables[i].present) continue;

        page_table_t *src_table = src->ref_tables[i];
        page_table_t *new_table = (page_table_t*) dumb_kmalloc(sizeof(page_table_t), 1);
        memset(new_table, 0, sizeof(page_table_t));

        for (uint32_t j = 0; j < 1024; j++) {
            if (!src_table->pages[j].present) continue;
            count += 1;

            new_table->pages[j].frame = src_table->pages[j].frame;
            new_table->pages[j].present = 1;
            new_table->pages[j].rw = src_table->pages[j].rw;
            new_table->pages[j].user = 1; // src_table->pages[j].user;
        }

        uint32_t t = (uint32_t) virtual2physical(src, new_table);
        new_dir->tables[i].present = 1;
        new_dir->tables[i].rw = src->tables[i].rw;
        new_dir->tables[i].user = 1; //src->tables[i].user;
        new_dir->tables[i].frame = t >> 12;
        new_dir->ref_tables[i] = new_table;
    }

    return new_dir;
}

void free_page_directory(page_directory_t *dir) {
    for (uint32_t i = 0; i < 1024; i++) {
        if (!dir->tables[i].present) continue;

        page_table_t *table = dir->ref_tables[i];
        if (!table) continue;

        // for (uint32_t j = 0; j < 1024; j++) {
        //     if (!table->pages[j].present) continue;
        //     free_block(table->pages[j].frame);
        // }

        kfree(table);
    }

    kfree(dir);
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
    int count = 0;
    while (i < LOAD_MEMORY_ADDRESS + 4 * 0x100000) {
        allocate_page(kpage_dir, i, 0x3);
        i += BLOCK_SIZE;
        count += 1;
    }

    printf("pages allocated: %d\n", count);
    register_interrupt_handler(14, page_fault_handler);

    // Map some memory for the kernel heap
    while (i < LOAD_MEMORY_ADDRESS + 4 * 0x100000 + KHEAP_INITIAL_SIZE) {
        allocate_page(kpage_dir, i, 0x3);
        i += BLOCK_SIZE;
        count += 1;
    }
    printf("pages allocated: %d\n", count);


    // Map RSDP
    // i = LOAD_MEMORY_ADDRESS + 0x000E0000;
    // while (i < LOAD_MEMORY_ADDRESS + 0x00100000) {
    //     allocate_page(kpage_dir, i, 0x3);
    //     i += BLOCK_SIZE;
    // }

    // Switch to the new page directory
    switch_page_directory(kpage_dir);
    enable_paging();
}

void debug_page_mapping(page_directory_t *dir, uint32_t virtual_address) {
    uint32_t pd_index = virtual_address >> 22;
    uint32_t pt_index = (virtual_address >> 12) & 0x3FF;

    page_table_t *pt = dir->ref_tables[pd_index];
    if (pt) {
        uint32_t frame = pt->pages[pt_index].frame << 12;
        printf("Page Table Entry for %x: Physical Frame = %x, Present = %d, RW = %d, User = %d\n",
               virtual_address, frame, pt->pages[pt_index].present,
               pt->pages[pt_index].rw, pt->pages[pt_index].user);
    } else {
        printf("Page Table not found for virtual address %x\n", virtual_address);
    }
}

void dump_page_directory(page_directory_t *dir) {
    printf("=== Page Directory Dump (CR3 = %x) ===\n", dir);

    for (uint32_t i = 0; i < 1024; i++) {
        if (!dir->tables[i].present) continue;  // Skip unmapped entries

        printf("PDE[%d]: Frame: %x | Present: %d | RW: %d | User: %d\n",
               i, dir->tables[i].frame << 12, dir->tables[i].present,
               dir->tables[i].rw, dir->tables[i].user);

        page_table_t *table = dir->ref_tables[i];
        if (!table) continue;
        for (uint32_t j = 0; j < 1024; j++) {
            if (!table->pages[j].present) continue;

            uint32_t virt_addr = (i << 22) | (j << 12);
            uint32_t phys_addr = table->pages[j].frame << 12;

            printf("  PTE[%d]: Virt: %x -> Phys: %x | RW: %d | User: %d\n",
                   j, virt_addr, phys_addr, table->pages[j].rw, table->pages[j].user);
        }
    }
    printf("=====================================\n");
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