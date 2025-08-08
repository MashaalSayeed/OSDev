#include "kernel/isr.h"
#include "kernel/paging.h"
#include "kernel/pmm.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "drivers/serial.h"
#include "kernel/gdt.h"
#include "kernel/system.h"
#include "kernel/process.h"
#include "kernel/exceptions.h"

uint8_t * temp_mem;
page_directory_t *kpage_dir; // Kernel page directory

extern uint8_t *bitmap;
extern uint32_t bitmap_size;
extern uint8_t kernel_stack_bottom;

int page_fault_detected = 0;
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
        return (void *)((uint32_t)physical + LOAD_MEMORY_ADDRESS);
    }

    uint32_t phys_addr = (uint32_t)physical;
    uint32_t pd_index = phys_addr >> 22;  // Page directory index
    uint32_t pt_index = (phys_addr >> 12) & 0x03FF;  // Page table index
    uint32_t page_frame_offset = phys_addr & 0xFFF;  // Offset within page

    page_table_t *table = dir->ref_tables[pd_index];
    if (!table || !table->pages[pt_index].present) {
        printf("physical2virtual: page table entry does not exist\n");
        return NULL;
    }

    uint32_t virt_addr = (table->pages[pt_index].frame << 12) + page_frame_offset;
    return (void *)virt_addr;
}

// Get a page table entry for a given virtual address
// If 'make' is true, create the page table if it doesn't exist
// Returns NULL if the page table entry cannot be created
page_table_entry_t *get_page(uint32_t virtual, int make, page_directory_t *dir) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    if (!dir->ref_tables[pd_index]) {
        if (!make) return NULL;

        page_table_t *table = NULL;
        if (!paging_enabled) {
            table = (page_table_t*) dumb_kmalloc(sizeof(page_table_t), 1);
        } else {
            table = (page_table_t*) kmalloc_aligned(sizeof(page_table_t), PAGE_SIZE);
        }
        // TODO:
        if (!table) {
            printf("get_page: Failed to allocate page table for %x\n", virtual);
            return NULL;
        }
        memset(table, 0, sizeof(page_table_t));

        dir->tables[pd_index].present = 1;
        dir->tables[pd_index].rw = 1;
        dir->tables[pd_index].user = 1;
        dir->tables[pd_index].frame = (uint32_t)virtual2physical(dir, table) >> 12;
        dir->ref_tables[pd_index] = table;
    }

    return &dir->ref_tables[pd_index]->pages[pt_index];
}

// Allocate a page and set its flags
// If the page is already allocated, it will not reallocate
void alloc_page(page_table_entry_t *page, uint32_t flags) {
    if (page->present) {
        printf("alloc_page: Page already allocated %x\n", page);
        return;
    }

    uint32_t frame = pmm_alloc_block();
    page->frame = (uint32_t)frame;
    page->present = 1;
    page->rw = (flags & PAGE_RW) ? 1 : 0;
    page->user = (flags & PAGE_USER) ? 1 : 0;
}

void free_page(page_table_entry_t *page) {
    if (!page || !page->present) {
        printf("free_page: Page not present\n");
        return;
    }

    pmm_free_block((void *)(page->frame));
    memset(page, 0, sizeof(page_table_entry_t));
}

void map_memory(page_directory_t *dir, uint32_t virtual_start, uint32_t physical_start, uint32_t size, uint32_t flags) {
    uint32_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE; // Round up to full pages

    // if (physical_start && !IS_ALIGN(physical_start)) {
    //     // printf("Warning: physical_start is not page aligned, aligning it\n");
    //     physical_start = PAGE_ALIGN(physical_start);
    // }

    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t virtual_addr = virtual_start + (i * PAGE_SIZE);
        uint32_t physical_addr = (physical_start) ? (physical_start + (i * PAGE_SIZE)) : 0;

        // Request a page
        page_table_entry_t *page = get_page(virtual_addr, 1, dir);
        if (!page) {
            printf("map_memory: Failed to get page for %x\n", virtual_addr);
            continue;
        }

        if (page->present) {
            printf("map_memory: Page already present at %x (dir: %x)\n", virtual_addr, dir);
            continue;
        }

        // Allocate and map the frame if physical_start is provided
        if (physical_addr) {
            page->frame = physical_addr >> 12;
        } else {
            alloc_page(page, flags); // Allocate normally
        }

        page->present = 1;
        page->rw = (flags & PAGE_RW) ? 1 : 0;
        page->user = (flags & PAGE_USER) ? 1 : 0;
    }
}

void kmap_memory(uint32_t virtual_start, uint32_t physical_start, uint32_t size, uint32_t flags) {
    map_memory(kpage_dir, virtual_start, physical_start, size, flags);
}

void switch_page_directory(page_directory_t *dir) {
    // Set the CR3 register to the physical address of the page directory
    uint32_t phys = (page_directory_t*) virtual2physical(kpage_dir, dir);
    if (!phys) {
        printf("Failed to switch page directory\n");
        return;
    }

    asm volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}

uint32_t copy_from_page(page_directory_t* src, uint32_t virt) {
    switch_page_directory(src);
    uint32_t *tmp_virt = 0xFFF00000;
    page_table_entry_t *temp_page = get_page(tmp_virt, 1, src);
    alloc_page(temp_page, 0x7);
    memcpy((void *)tmp_virt, (void *)virt, PAGE_SIZE);

    uint32_t frame = temp_page->frame;
    temp_page->present = 0; // Mark as not present to avoid freeing it
    temp_page->frame = 0; // Clear the frame to avoid double free
    switch_page_directory(kpage_dir); // Switch back to the kernel page directory
    return frame;
}

// TODO: Implement copy-on-write
page_directory_t * clone_page_directory(page_directory_t *src) {
    page_directory_t *new_dir = (page_directory_t*) kmalloc_aligned(sizeof(page_directory_t), PAGE_SIZE);
    if (!new_dir) {
        printf("Failed to allocate new page directory\n");
        return NULL;
    }

    memset(new_dir, 0, sizeof(page_directory_t));
    for (uint32_t i = 0; i < 1024; i++) {
        if (!src->tables[i].present) continue;

        page_table_t *src_table = src->ref_tables[i];

        // Copy kernel mappings by reference
        if (i >= 768 && src_table && kpage_dir->ref_tables[i] == src_table) {
            new_dir->tables[i] = src->tables[i];
            new_dir->ref_tables[i] = src_table;
            continue;
        }

        page_table_t *new_table = (page_table_t*) kmalloc_aligned(sizeof(page_table_t), PAGE_SIZE);
        memset(new_table, 0, sizeof(page_table_t));

        for (uint32_t j = 0; j < 1024; j++) {
            if (!src_table->pages[j].present) continue;

            uint32_t virt_addr = i << 22 | j << 12;
            uint32_t phys_addr = src_table->pages[j].frame;
            new_table->pages[j].frame = copy_from_page(src, virt_addr);
            new_table->pages[j].present = 1;
            new_table->pages[j].rw = 1; // 0 for COW
            new_table->pages[j].user = 1; // src_table->pages[j].user;
            // printf("Copied page %x (%x -> %x)\n", virt_addr, phys_addr, new_table->pages[j].frame);
        }

        uint32_t t = (uint32_t) virtual2physical(src, new_table);
        new_dir->tables[i].present = 1;
        new_dir->tables[i].rw = src->tables[i].rw; // 0 for COW
        new_dir->tables[i].user = src->tables[i].user;
        new_dir->tables[i].frame = t >> 12;
        new_dir->ref_tables[i] = new_table;
    }

    return new_dir;
}

void free_page_directory(page_directory_t *dir) {
    if (!dir || dir == kpage_dir) return;
    for (uint32_t i = 0; i < 1024; i++) {
        if (!dir->tables[i].present) continue;

        page_table_t *table = dir->ref_tables[i];
        if (!table || kpage_dir->ref_tables[i] == table) continue;

        for (uint32_t j = 0; j < 1024; j++) {
            if (!table->pages[j].present) continue;
            free_page(&table->pages[j]);
        }

        kfree_aligned(table);
    }

    kfree_aligned(dir);
}

void enable_paging() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    paging_enabled = 1;
}

void invalidate_page(uint32_t addr) {
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

void get_cr3(uint32_t *cr3) {
    asm volatile("mov %%cr3, %0" : "=r"(*cr3));
}

void paging_init() {
    temp_mem = bitmap + bitmap_size;

    // Remove the temporary page directory in kernel's data section and install new one
    kpage_dir = (page_directory_t*) dumb_kmalloc(sizeof(page_directory_t), 1);
    memset(kpage_dir, 0, sizeof(page_directory_t));

    register_interrupt_handler(14, page_fault_handler);

    // Map the first 4MB of memory to the first 4MB of physical memory
    // 768 - 1024 is reserved for the kernel stack and other kernel data
    kmap_memory(LOAD_MEMORY_ADDRESS, 0, 4 * 0x100000, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    // Map some memory for the kernel heap
    kmap_memory(LOAD_MEMORY_ADDRESS + 4 * 0x100000, 0, KHEAP_INITIAL_SIZE, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    // Setup a guard page for the kernel stack
    // free_page(get_page((uint32_t) &kernel_stack_bottom + BLOCK_SIZE, 0, kpage_dir));

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
        // for (uint32_t j = 0; j < 1024; j++) {
        //     if (!table->pages[j].present) continue;

        //     uint32_t virt_addr = (i << 22) | (j << 12);
        //     uint32_t phys_addr = table->pages[j].frame << 12;

        //     printf("  PTE[%d]: Virt: %x -> Phys: %x | RW: %d | User: %d\n",
        //            j, virt_addr, phys_addr, table->pages[j].rw, table->pages[j].user);
        // }
    }
    printf("=====================================\n");
}

void page_fault_handler(registers_t *regs) {
    if (!paging_enabled) {
        printf("Page fault occurred but paging is not enabled!\n");
        for (;;) ;
    }
    if (page_fault_detected) {
        printf("Nested page fault detected! Halting system.\n");
        for (;;) ;
    }
    page_fault_detected = 1;


    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

    if (faulting_address >= &kernel_stack_bottom && faulting_address < &kernel_stack_bottom + BLOCK_SIZE) {
        printf("Stack overflow at %x\n", faulting_address);
        for (;;) ;
    }

    uint32_t cr3;
    get_cr3(&cr3);

    printf("Page fault at %x\n", faulting_address);
    printf("Error code: %x\n", regs->err_code);
    printf("Page Directory: %x (kernel: %s)\n", cr3, cr3 == (uint32_t)kpage_dir ? "yes" : "no");
    print_debug_info(regs);
    
    process_t *proc = get_current_process();
    if (proc) printf("PID: %d\n", proc->pid);

    uint32_t present = regs->err_code & PF_ERR_PRESENT;
    uint32_t rw = regs->err_code & PF_ERR_RW;
    uint32_t user = regs->err_code & PF_ERR_USER;
    uint32_t reserved = regs->err_code & PF_ERR_RESERVED;
    uint32_t inst_fetch = regs->err_code & PF_ERR_INST;

    printf("Possible causes: [ ");
    if(!present) printf("Page not present ");
    if(rw) printf("Page is read only ");
    if(user) printf("Page is read only ");
    if(reserved) printf("Overwrote reserved bits ");
    if(inst_fetch) printf("Instruction fetch ");
    printf("]\n");

    print_stack_trace(regs);

    // for (;;) ;
}