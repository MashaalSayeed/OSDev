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
#include "kernel/acpi.h"
#include "kernel/framebuffer.h"
#include "kernel/process.h"
#include "kernel/tests.h"
#include "kernel/printf.h"
#include "kernel/vfs.h"
#include "kernel/elf.h"
#include "libc/string.h"
#include "drivers/pci.h"
#include "drivers/ata.h"
#include "drivers/rtc.h"
#include "kernel/shm.h"
#include "kernel/wm_dev.h"
#include "drivers/mouse.h"

bool is_gui_enabled = false;
elf_t kernel_elf;
framebuffer_t fb_data;
extern page_directory_t* kpage_dir;

static void multiboot2_init(struct multiboot_tag* mbd) {
	struct multiboot_tag* tag = mbd;
	struct multiboot_tag_framebuffer* fb;
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
			is_gui_enabled = fb->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB;
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

void init_main();
void process_test();


void kernel_main(uint32_t magic, struct multiboot_tag* mbd) 
{
	gdt_install();
	isr_install();
	asm volatile("sti");

	init_serial();
	serial_write("Initialized Serial\n");

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

	paging_init();
	printf("Allocating 1096MB of memory\n");
	kheap_init();
	printf("Initialized Paging!!\n\n");

	
	pci_init();
	ata_init();
	// acpi_init();
	printf("\n");


	// TODO: Load kernel ELF file at higher virtual address
	kmap_memory((uint32_t)kernel_elf.symtab, (uint32_t)kernel_elf.symtab, kernel_elf.symtabsz, 0x7);
	kmap_memory((uint32_t)kernel_elf.strtab, (uint32_t)kernel_elf.strtab, kernel_elf.strtabsz, 0x7);

	vfs_init();
	scheduler_init();

	printf("kernel page dir: %x\n", kpage_dir);

	process_t *init_proc = create_process("init", init_main, PROCESS_FLAG_KERNEL);
	process_t *test_proc = create_process("test", process_test, PROCESS_FLAG_KERNEL);

	schedule_process_threads(init_proc);
	schedule_process_threads(test_proc);


	if (is_gui_enabled) {
		/*
		 * User-space compositor path:
		 *  1. Init the physical framebuffer (allocates backbuffer in kernel
		 *     heap, maps FB VA, etc.).
		 *  2. Init the SHM subsystem so clients can share pixel buffers.
		 *  3. Register /DEV/WM (must run after vfs_init mounts devfs).
		 *  4. Init PS/2 mouse and start the input-bridge kernel thread so
		 *     mouse + keyboard events reach the compositor via /DEV/WM.
		 *  5. exec("/BIN/COMPOSITOR") is triggered by init_main below.
		 */
		init_framebuffer(&fb_data);
		shm_init();
		wm_dev_init();
		ps2_mouse_init();

		kprintf(DEBUG, "GUI subsystems ready – launching compositor\n");
		jmp_to_kernel_thread(init_proc->main_thread);
	} else {
		kprintf(DEBUG, "No GUI\n");
		jmp_to_kernel_thread(init_proc->main_thread);
	}

	for (;;) {
		asm volatile("hlt");
	}
}

void init_main() {
	printf("Hello from init process!\n");
	if (is_gui_enabled) {
		exec("/BIN/DESKTOP", NULL);
	} else {
		exec("/BIN/SHELL", NULL);
	}
	/* If exec fails, spin */
	for (;;) asm volatile("hlt");
}

void process_test() {
	printf("Hello from test process!\n");
	uint32_t i = 0;
	for (;;) {
		// asm volatile ("cli");
		// printf("Hello from test process %d!\n", i);
		asm volatile ("sti");
		i++;
		asm volatile("hlt");
	}
}