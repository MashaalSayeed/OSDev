#include "kernel/memory.h"
#include "kernel/multiboot.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "libc/memory.h"

static uint32_t page_frame_min;
static uint32_t page_frame_max;
static uint32_t totalAlloc;
int mem_num_vpages;

#define NUM_PAGE_DIRS 256
#define NUM_PAGE_FRAMES (MEMORY_SIZE / PAGE_SIZE)

// Bitmap of physical memory
uint8_t physical_memory_bitmap[NUM_PAGE_FRAMES / 8];
static uint32_t page_directory[NUM_PAGE_DIRS][1024] __attribute__((aligned(4096)));
static uint32_t page_directory_used[NUM_PAGE_DIRS];

void pmm_init(uint32_t mem_lower, uint32_t mem_upper) {
    printf("Memory Lower: %x\n", mem_lower);
    printf("Memory Upper: %x\n", mem_upper);

    page_frame_min = CEIL_DIV(mem_lower, PAGE_SIZE);
    page_frame_max = mem_upper / PAGE_SIZE;
    totalAlloc = 0;

    memset(physical_memory_bitmap, 0, sizeof(physical_memory_bitmap));
}

void init_memory(struct multiboot_info *mbd) {
    uint32_t mods_addr = *(uint32_t *)(mbd->mods_addr + 4);
    uint32_t phys_alloc_start = (mods_addr + 0xFFF) & ~0xFFF;
    uint32_t phys_alloc_end = mbd->mem_upper * 1024;

    initial_page_dir[0] = 0;
    invalidate_page(0); // Invalidate the first page

    initial_page_dir[1023] = (uint32_t)(initial_page_dir - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_RW;
    invalidate_page(0xFFFFF000); // Invalidate the last page

    pmm_init(phys_alloc_start, phys_alloc_end);
    memset(page_directory, 0, sizeof(page_directory));
    memset(page_directory_used, 0, sizeof(page_directory_used));

    register_interrupt_handler(14, page_fault_handler);
}

void print_memory_map(struct multiboot_info *mbd) {
    for (uint32_t i=0; i < mbd->mmap_length; i += sizeof(struct multiboot_mmap_entry)) {
        struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry*) (mbd->mmap_addr + i);
        printf("Low Addr: %x | High Addr: %x | Low Len: %x | High Len: %x | Size: %d | Type: %d\n", 
            entry->addr_low, entry->addr_high, entry->len_low, entry->len_high, entry->size, entry->type);
    }
}

// Allocate a page
void* allocate_page() {
    uint32_t start = page_frame_min / 8 + (page_frame_min & 7 != 0 ? 1 : 0);
    uint32_t end = page_frame_max / 8 - (page_frame_max & 7 != 0 ? 1 : 0);

    for (uint32_t i=start; i < end; i++) {
        uint8_t byte = physical_memory_bitmap[i];
        if (byte == 0xFF) continue; // Byte is full

        for (uint32_t j=0; j < 8; j++) {
            if ((byte & (1 << j)) == 0) {
                byte |= (1 << j);
                physical_memory_bitmap[i] = byte;
                totalAlloc++;

                return (void *)((i * 8 + j) * PAGE_SIZE);
            }
        }
    }

    return 0;
}

// Map a virtual address to a physical address
void memmap_page(uint32_t virtual_addr, uint32_t phys_addr, uint32_t flags) {
    uint32_t *prev_page_dir = 0;
    if (virtual_addr >= KERNEL_START) {
        prev_page_dir = mem_current_page_dir();
        if (prev_page_dir != initial_page_dir) {
            mem_change_page_dir(initial_page_dir);
        }
    }

    uint32_t page_dir_index = virtual_addr >> 22;
    uint32_t page_table_index = (virtual_addr >> 12) & 0x3FF;
    uint32_t* page_directory = REC_PAGEDIR;
    uint32_t* page_table = REC_PAGETABLE(page_dir_index);

    if (!(page_directory[page_dir_index] & PAGE_FLAG_PRESENT)){
        uint32_t ptPAddr = allocate_page();
        page_directory[page_dir_index] = ptPAddr | PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_OWNER | flags;
        invalidate_page(virtual_addr);

        for (uint32_t i = 0; i < 1024; i++){
            page_table[i] = 0;
        }
    }

    page_table[page_table_index] = phys_addr | PAGE_FLAG_PRESENT | flags;
    mem_num_vpages++;
    invalidate_page(virtual_addr);

    if (prev_page_dir != 0){
        // syncPageDirs();

        if (prev_page_dir != initial_page_dir){
            mem_change_page_dir(prev_page_dir);
        }
    }

}

// Get the current page directory
void* mem_current_page_dir() {
    uint32_t *page_dir;
    asm volatile("mov %%cr3, %0" : "=r"(page_dir));
    page_dir += KERNEL_START;

    return page_dir;
}

// Change the current page directory
void mem_change_page_dir(uint32_t *page_dir) {
    page_dir -= KERNEL_START;
    asm volatile("mov %0, %%cr3" :: "r"(page_dir));
}

void sync_page_dirs() {
    for (int i = 0; i < NUM_PAGE_DIRS; i++) {
        if (page_directory_used[i] == 0) continue;

        uint32_t *page_dir = page_directory[i];
        for (int i = 768; i < 1023; i++){
            // page_directory[i] = initial_page_dir[i] & ~PAGE_FLAG_OWNER;
        }
    }
}

void free_page(void *ptr) {
    uint32_t addr = (uint32_t)ptr;
    if (addr % PAGE_SIZE != 0) {
        return; // Address is not page-aligned
    }

    size_t index = addr / PAGE_SIZE;
    physical_memory_bitmap[index / 8] &= ~(1 << (index % 8)); // Mark the page as free
}

void invalidate_page(uint32_t addr) {
    asm volatile("invlpg %0" :: "m"(addr));
}


void page_fault_handler(registers_t *regs) {
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

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