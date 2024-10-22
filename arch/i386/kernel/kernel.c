#include "drivers/tty.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "kernel/multiboot.h"
#include "kernel/pmm.h"
#include "kernel/paging.h"
#include "libc/stdio.h"
#include "kernel/gdt.h"
#include "kernel/isr.h"
#include "kernel/timer.h"

// void test_divide_by_zero() {
// 	int x = 1;
// 	int y = 0;
// 	int z = x / y;
// }

void kernel_main(uint32_t magic, struct multiboot_info* mbd) 
{
	gdt_install();
	isr_install();
	asm volatile("sti");

	init_serial();
	terminal_initialize();
	init_keyboard();

	// Allocate 1096MB of memory
	pmm_init(mbd, 1096 * 0x100000);
	paging_init();


	// init_memory(mbd);
	printf("\n");

	// void *ptr1 = kmalloc(100);
	// void *ptr2 = kmalloc(100);
	// printf("Allocated memory at %x\n", ptr1);
	// printf("Allocated memory at %x\n", ptr2);


	// printf("Multiboot flags: %x\n", mbd->flags);
	// test_divide_by_zero();
	// terminal_writestring("Enter input >");

	// init_timer(100);

	log_to_serial("Hello, serial World!\n");

	for (;;) ;
}