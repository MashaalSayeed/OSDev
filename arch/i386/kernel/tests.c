#include "kernel/tests.h"
#include "kernel/kheap.h"
#include "kernel/process.h"
#include "kernel/timer.h"
#include "kernel/locks.h"
#include "system.h"
#include "kernel/printf.h"
#include "kernel/vfs.h"
#include "kernel/elf.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include <stdarg.h>

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

uint32_t get_esp() {
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    return esp;
}

void uprintf(const char* format, ...) {
	va_list args;
	va_start(args, format);

	char buffer[80];
	int ret = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

    asm volatile (
        "movl $1, %%eax;"  // Syscall number for write (example)
        "movl %0, %%ebx;"  // File descriptor
        "movl %1, %%ecx;"  // Buffer pointer
        "movl %2, %%edx;"  // Size
        "int $0x80;"       // Trigger interrupt for syscall
        :
        : "g"(1), "g"(buffer), "g"(ret)
        : "eax", "ebx", "ecx", "edx"
    );
}

void fgets(int fd, char* buffer, int size) {
	asm volatile (
		"int $0x80"             // Trigger system call
		: "=a" (size)         // Return value in eax
		: "a" (2),      // Syscall number in eax
		  "b" (fd),             // File descriptor in ebx
		  "c" (buffer),         // Buffer address in ecx
		  "d" (size)            // Size in edx
		: "memory"
	);
	// uprintf("fgets: %x\n", size);
}

spinlock_t lock;
void test_process1() {
	int a = 0;
    while (1) {
		uprintf("Hello from process 1!\n");
		sleep(10);
		// spinlock_acquire(&lock);
        // uprintf("Process 1: ESP = %x\n", get_esp());
		// spinlock_release(&lock);
    }
}

void test_process2() {
	while (1) {
		// spinlock_acquire(&lock);
		// printf("Process 2: ESP = %x\n", get_esp());
		uprintf("Hello from process 2!\n");
		sleep(10);
		// spinlock_release(&lock);
	}
}

void exit(int arg) {
	asm volatile (
		"int $0x80"
		:
		: "a"(5), "b"(arg)
		: "memory"
	);
}

void shell() {
	while (1) {
		char buffer[80];
		uprintf("> ");
		sleep(80);
		break;
		// fgets(0, buffer, 80);
		// uprintf("You entered: %s\n", buffer);

		// if (strcmp(buffer, "exit") == 0) {
		// 	break;
		// }
	}

	exit(0);
}

void test_scheduler() {
	// initialize_lock(&lock);

	// process_t* process1 = create_process("process1", &test_process1);
	// add_process(process1);

	// process_t* process2 = create_process("process2", &test_process2);
	// add_process(process2);

	process_t* process3 = create_process("shell", &shell);
	add_process(process3);

	print_process_list();
}

void test_elf_loader() {
	process_t* user_process = load_elf("/user.bin");
	if (user_process == NULL) {
		printf("Failed to load ELF file\n");
		return;
	}

	add_process(user_process);
	printf("Loaded ELF file\n");
}