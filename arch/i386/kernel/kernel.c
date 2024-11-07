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
#include "libc/stdio.h"
#include "system.h"

extern page_directory_t *kpage_dir;

void test_divide_by_zero() {
	int x = 1;
	int y = 0;
	int z = x / y;
	UNUSED(z);
}

void test_heap() {
	uint8_t* ptr1 = (uint8_t *)kmalloc(0x20);
	uint8_t* ptr2 = (uint8_t *)kmalloc(0x20);

	printf("Allocated memory at %x\n", ptr1);
	printf("Allocated memory at %x\n", ptr2);

	kfree(ptr2);
	printf("Freed memory at %x\n", ptr2);

	ptr2 = kmalloc(0x20);
	printf("Allocated memory at %x\n\n", ptr2);

	print_kheap();
	kfree(ptr1);
	kfree(ptr2);
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color, struct multiboot_info* mbd) {
    uint32_t* framebuffer = (uint32_t*)mbd->framebuffer_addr;
    uint32_t pitch = mbd->framebuffer_pitch / 4; // pitch in pixels
    framebuffer[y * pitch + x] = color;
}


void kernel_main(uint32_t magic, struct multiboot_info* mbd) 
{
	gdt_install();
	isr_install();
	asm volatile("sti");

	init_serial();
	log_to_serial("Initialized Serial\n");

	terminal_initialize();
	printf("Hello World\n");

	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printf("Invalid magic number: 0x%x\n", magic);
		return;
	}

	if (!(mbd->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
		printf("No framebuffer available\n");
		log_to_serial("No framebuffer available\n");
		return;
	}

	init_keyboard();

	// Allocate 1096MB of memory
	printf("Allocating 1096MB of memory\n");
	pmm_init(mbd, 1096 * 0x100000);
	printf("Initialized Physical Memory Management\n\n");

	printf("Initialize Paging\n");
	paging_init();
	kheap_init();
	printf("Initialized Paging\n\n");
	
	acpi_init();

	log_to_serial("Hello, Serial World 1!\n");

	uint32_t red = 0xFF0000; // Assuming 32-bit color depth (ARGB)

	printf("Multiboot Descriptor: %x\n", mbd);
	map_physical_to_virtual(mbd + 0xC0000000, mbd);

	mbd += 0xC0000000;

	printf("Multiboot Descriptor: %x\n", mbd);
	printf("Multiboot Framebuffer Address: %x\n", mbd->framebuffer_addr);
    // draw_pixel(10, 10, red, mbd);

	// find_rsdt();

	// init_timer(100);
	// test_divide_by_zero();

	log_to_serial("Hello, Serial World 2!\n");

	for (;;) ;
}