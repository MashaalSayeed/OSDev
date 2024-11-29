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

#include "image.h"

extern page_directory_t *kpage_dir;

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

	struct multiboot_tag* tag = mbd;
	struct multiboot_tag_framebuffer fb;
	mbd->size = sizeof(struct multiboot_tag); // idk why this is needed
    for (; tag->type != MULTIBOOT_TAG_TYPE_END; 
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) 
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME) {
            struct multiboot_tag_string* boot_loader_name = (struct multiboot_tag_string*) tag;
            printf("Bootloader Name: %s\n", boot_loader_name->string);
        }
        else if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
			struct multiboot_tag_framebuffer *fb_tag = (struct multiboot_tag_framebuffer*) tag;
            printf("Framebuffer pitch: %d, width: %d, height: %d, bpp: %d\n",
                   fb_tag->framebuffer_pitch, fb_tag->framebuffer_width,
                   fb_tag->framebuffer_height, fb_tag->framebuffer_bpp);
			printf("Framebuffer address: %x\n", fb_tag->framebuffer_addr);
			fb = *fb_tag;
        }
    }

	printf("\n");
	init_keyboard();

	// Allocate 1096MB of memory
	printf("Allocating 1096MB of memory\n");
	pmm_init(mbd, 1096 * 0x100000);
	printf("Initialized Physical Memory Management\n\n");

	printf("Initializing Paging\n");
	paging_init();
	kheap_init();
	printf("Initialized Paging!!\n\n");
	
	// acpi_init();

	log_to_serial("Hello, Serial World 1!\n");

	// map_physical_to_virtual_region(fb.framebuffer_addr, fb.framebuffer_addr, fb.framebuffer_pitch * fb.framebuffer_height);
	// init_framebuffer(fb.framebuffer_width, fb.framebuffer_height, fb.framebuffer_pitch, fb.framebuffer_bpp, fb.framebuffer_addr);
	// printf("Multiboot Framebuffer Address: %x\n", fb.framebuffer_addr);

	// psf_font_t *font = load_psf_font();
	// if (!font) {
	// 	printf("Failed to load font\n");
	// 	return;
	// }


	// uint32_t white = 0xFFFFFF;
	// uint32_t black = 0x000000;
	// printf("Font Address: %x\n", font);
	// draw_string_at("Hello World!\nReally?", 0, 0, white, black);
	// scroll(black);

	// draw_image(image_data, 0, 0, IMAGE_DATA_WIDTH, IMAGE_DATA_HEIGHT);

	// find_rsdt();
	printf("Creating processes\n");
	terminal_clear();
	scheduler_init();
	// print_process_list();
	init_timer(100);

	test_scheduler();

	log_to_serial("Hello, Serial World 2!\n");

	for (;;) ;
}