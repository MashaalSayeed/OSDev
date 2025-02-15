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
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/vfs.h"
#include "drivers/pci.h"
#include "drivers/ata.h"
#include "image.h"

#define USER_PROGRAM_START 0xB0000000

typedef void (*user_func_t)(void);

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
			// printf("Framebuffer type: %d\n", fb->framebuffer_type);
			// printf("Framebuffer pitch: %d, width: %d, height: %d, bpp: %d\n",
			// 	   fb->framebuffer_pitch, fb->framebuffer_width,
			// 	   fb->framebuffer_height, fb->framebuffer_bpp);
			// printf("Framebuffer address: %x\n", fb->framebuffer_addr);

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
		else if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
			struct multiboot_tag_module* module = (struct multiboot_tag_module*) tag;
			printf("Module: %s\n", module->cmdline);
			printf("Module start: %x, end: %x\n", module->mod_start, module->mod_end);
			// load_program_to_userspace((void*)module->mod_start, module->mod_end - module->mod_start);
		}
		else if (tag->type == MULTIBOOT_TAG_TYPE_ELF_SECTIONS) {
			struct multiboot_tag_elf_sections* elf_sections = (struct multiboot_tag_elf_sections*) tag;
			printf("ELF Sections: %d\n", elf_sections->num);
		}
		else if (tag->type == MULTIBOOT_TAG_TYPE_BASIC_MEMINFO) {
			struct multiboot_tag_basic_meminfo* meminfo = (struct multiboot_tag_basic_meminfo*) tag;
			printf("Memory: %dKB lower, %dKB upper\n", meminfo->mem_lower, meminfo->mem_upper);
		}
		else {
			// printf("Unknown tag: %d\n", tag->type);
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
	
	pci_init();
	ata_init();
	// acpi_init();
	printf("\n");

	debug_page_mapping(kpage_dir, 0xC0013346);
	debug_page_mapping(kpage_dir, 0xC00143aa);

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
	// test_elf_loader();

	log_to_serial("Hello, Serial World 2!\n");

	for (;;) ;
}

void load_program_to_userspace(void* program_binary, size_t size) {
    uint32_t addr = USER_PROGRAM_START;
    for (size_t i = 0; i < size; i += BLOCK_SIZE) {
		map_physical_to_virtual(addr + i, program_binary + i);
    }
}
