#include "drivers/tty.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "kernel/multiboot.h"
#include "kernel/memory.h"
#include "libc/stdio.h"
#include "gdt.h"
#include "isr.h"
#include "timer.h"

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
	init_memory(mbd);

	printf("Hello, kernel World!\n");
	printf("Magic number: %x\n", magic);
	printf("%x\n", 0x64);
	printf("%d\n", 0x64);
	// terminal_writestring("Enter input >");

	// init_timer(100);

	log_to_serial("Hello, serial World!\n");

	for (;;) ;
}