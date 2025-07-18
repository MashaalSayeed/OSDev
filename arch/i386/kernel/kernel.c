#include <stdbool.h>

#include "drivers/tty.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "kernel/multiboot.h"
#include "kernel/pmm.h"
#include "kernel/paging.h"
#include "kernel/kheap.h"
#include "kernel/gdt.h"
#include "kernel/isr.h"
#include "kernel/acpi.h"
#include "kernel/framebuffer.h"
#include "kernel/font.h"
#include "kernel/process.h"
#include "kernel/tests.h"
#include "kernel/printf.h"
#include "kernel/vfs.h"
#include "kernel/elf.h"
#include "libc/string.h"
#include "drivers/pci.h"
#include "drivers/ata.h"
#include "drivers/rtc.h"
#include "kernel/gui.h"

bool is_gui_enabled = false;
elf_t kernel_elf;
framebuffer_t fb_data;
extern page_directory_t* kpage_dir;

static void multiboot2_init(struct multiboot_tag* mbd) {
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
			fb_data = framebuffer_from_multiboot(fb);

			// framebuffer = init_framebuffer(fb->framebuffer_width, fb->framebuffer_height, fb->framebuffer_pitch, fb->framebuffer_bpp, fb->framebuffer_addr);
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
			kernel_elf = elf_from_multiboot(elf_sections);
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

void print_time() {
	rtc_time_t time = rtc_read_time();
	kprintf(INFO, "Current Time: %02d:%02d:%02d %02d/%02d/%02d\n", time.hour, time.minute, time.second, time.day, time.month, time.year);
}


void kernel_main(uint32_t magic, struct multiboot_tag* mbd) 
{
	gdt_install();
	isr_install();
	asm volatile("sti");

	init_serial();
	log_to_serial("Initialized Serial\n");

	terminal_initialize();
	kprintf(DEBUG, "Initialized Terminal\n\n");
	print_time();

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kprintf(ERROR, "Invalid magic number: %x\n", magic);
        return;
    }

	multiboot2_init(mbd);

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


	// TODO: Load kernel ELF file at higher virtual address
	kmap_memory(kernel_elf.symtab, kernel_elf.symtab, kernel_elf.symtabsz, 0x7);
	kmap_memory(kernel_elf.strtab, kernel_elf.strtab, kernel_elf.strtabsz, 0x7);

	init_timer(100);

	if (is_gui_enabled) {
		gui_init(&fb_data);
		gui_loop();
	} else {
		kprintf(DEBUG, "No GUI\n");
	}

	scheduler_init();

	vfs_init();

	// find_rsdt();

	// test_scheduler();
	// test_fork();
	// test_shell();
	exec("/BIN/SHELL", NULL);
	// test_heap();
	// test_string();

	// exec("/BIN/HELLO", NULL);


	for (;;) {
		asm volatile("hlt");
	}
}