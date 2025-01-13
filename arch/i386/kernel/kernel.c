#include <stdbool.h>

#include "drivers/tty.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "kernel/multiboot.h"
#include "kernel/pmm.h"
#include "kernel/paging.h"
#include "kernel/kheap.h"
#include "kernel/gdt.h"
#include "kernel/isr.h"
#include "kernel/timer.h"
#include "kernel/acpi.h"
#include "kernel/framebuffer.h"
#include "kernel/font.h"
#include "kernel/process.h"
#include "kernel/tests.h"
#include "libc/stdio.h"
#include "kernel/vfs.h"
#include "drivers/pci.h"
#include "drivers/ata.h"
#include "image.h"


#define USER_PROGRAM_START 0x40000000
#define USER_STACK_START 0xC0000000
#define USER_STACK_SIZE 0x1000

uint8_t program_binary[] = {0xf4, 0xeb, 0xfd};
size_t program_binary_size = sizeof(program_binary);
extern page_directory_t *kpage_dir;

framebuffer_t* framebuffer;
bool is_gui_enabled = false;

void multiboot2_init(struct multiboot_tag* mbd) {
	struct multiboot_tag* tag = mbd;
	struct multiboot_tag_framebuffer* fb;
	struct multiboot_tag_string* boot_loader_name;
	struct multiboot_tag_mmap* mmap_tag;

	mbd->size = sizeof(struct multiboot_tag); // idk why this is needed
	for (; tag->type != MULTIBOOT_TAG_TYPE_END; 
		 tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) 
	{
		if (tag->type == MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME) {
			struct multiboot_tag_string* boot_loader_name = (struct multiboot_tag_string*) tag;
			printf("Bootloader Name: %s\n", boot_loader_name->string);
		}
		else if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
			fb = (struct multiboot_tag_framebuffer*) tag;
			printf("Framebuffer type: %d\n", fb->framebuffer_type);
			printf("Framebuffer pitch: %d, width: %d, height: %d, bpp: %d\n",
				   fb->framebuffer_pitch, fb->framebuffer_width,
				   fb->framebuffer_height, fb->framebuffer_bpp);
			printf("Framebuffer address: %x\n", fb->framebuffer_addr);

			framebuffer = init_framebuffer(fb->framebuffer_width, fb->framebuffer_height, fb->framebuffer_pitch, fb->framebuffer_bpp, fb->framebuffer_addr);
			is_gui_enabled = fb->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB;
		}
		else if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
			mmap_tag = (struct multiboot_tag_mmap*) tag;
			struct multiboot_mmap_entry * entry = mmap_tag->entries;
			while ((uint32_t)entry < (uint32_t)mmap_tag + mmap_tag->size) {
				// printf("Memory: %x - %x, Type: %d\n", entry->addr, entry->addr + entry->len, entry->type);
				// if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
				// 	add_memory_region(entry->addr, entry->len); // Add memory region to the bitmap
				// }

				entry = (struct multiboot_mmap_entry*)((uint32_t)entry + mmap_tag->entry_size);
			}
		}
	}
}

void kernel_main(uint32_t magic, struct multiboot_tag* mbd) 
{
	gdt_install();
	isr_install();
	asm volatile("sti");

	init_serial();
	log_to_serial("Initialized Serial\n");

	terminal_initialize();
	printf("Initialized Terminal\n\n");

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: %x\n", magic);
        return;
    }

	multiboot2_init(mbd);

	printf("\n");
	init_keyboard();

	// Allocate 1096MB of memory
	printf("Allocating 1096MB of memory\n");
	pmm_init(mbd, 1096 * 0x100000);
	printf("Initialized Physical Memory Management\n\n");

	paging_init();
	kheap_init();
	printf("Initialized Paging!!\n\n");
	
	// pci_init();
	ata_init();
	// acpi_init();
	printf("\n");

	map_physical_to_virtual_region(framebuffer->addr, framebuffer->addr, framebuffer->pitch * framebuffer->height);
	log_to_serial("Hello, Serial World 1!\n");

	if (is_gui_enabled) {
		psf_font_t *font = load_psf_font();
		fill_screen(0x000000);
		draw_image(image_data, 0, 0, IMAGE_DATA_WIDTH, IMAGE_DATA_HEIGHT);
		draw_string_at("Hello, GUI World!", 0, 0, 0xFFFFFF, 0x000000);
	} else {
		printf("No GUI\n");
	}


	// find_rsdt();
	// printf("Creating processes\n");
	// terminal_clear();
	scheduler_init();
	// print_process_list();
	// init_timer(100);

	// test_scheduler();

	vfs_init();

	// load_program_to_userspace(program_binary, program_binary_size);
	// printf("%d\n", USER_PROGRAM_START);
	// switch_to_user_mode(USER_STACK_START, USER_PROGRAM_START);

	log_to_serial("Hello, Serial World 2!\n");

	for (;;) ;
}

void load_program_to_userspace(void* program_binary, size_t size) {
    uint32_t addr = USER_PROGRAM_START;
    uint8_t* binary = (uint8_t*)program_binary;

    for (size_t i = 0; i < size; i += BLOCK_SIZE) {
        uint32_t phys_page = allocate_block();
		map_physical_to_virtual(addr + i, phys_page);
        memcpy((void*)(addr + i), binary + i, BLOCK_SIZE); // Assuming identity map for kernel
    }

	// Map memory for the user stack
    for (uint32_t addr = USER_STACK_START - USER_STACK_SIZE; addr < USER_STACK_START; addr += BLOCK_SIZE) {
		map_physical_to_virtual(addr, allocate_block());
    }
}

__attribute__((naked, noreturn))
void switch_to_user_mode(uint32_t stack_addr, uint32_t code_addr) {
    asm volatile (
		"cli\n"           // Disable interrupts
		"push $0x23\n"   // Data segment selector
		"push %0\n"      // Stack pointer
		"push $0x202\n"  // EFlags
		"push $0x1B\n"   // Code segment selector
		"push %1\n"      // Instruction pointer
		"iret \n"
		:
		: "r"(stack_addr), "r"(code_addr)
	);
}