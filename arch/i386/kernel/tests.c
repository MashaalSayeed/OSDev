#include "kernel/tests.h"
#include "kernel/kheap.h"
#include "kernel/process.h"
#include "kernel/timer.h"
#include "kernel/locks.h"
#include "system.h"
#include "kernel/printf.h"
#include "kernel/vfs.h"
#include "kernel/elf.h"

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

spinlock_t lock;
void test_process1() {
	int a = 0;
    while (1) {
		// uprintf("Hello from process 1!\n");
		sleep(10);
		// spinlock_acquire(&lock);
        // printf("Process 1: ESP = %x\n", get_esp());
		// spinlock_release(&lock);
    }
}

void test_process2() {
	while (1) {
		// spinlock_acquire(&lock);
		// printf("Process 2: ESP = %x\n", get_esp());
		// uprintf("Hello from process 2!\n");
		sleep(10);
		// spinlock_release(&lock);
	}
}

void test_process3() {
	while (1) {
		// spinlock_acquire(&lock);
		// printf("Process 3: ESP = %x\n", get_esp());
		sleep(80);
		// spinlock_release(&lock);
	}
}

void test_scheduler() {
	// initialize_lock(&lock);

	// process_t* process1 = create_process("process1", &test_process1);
	// printf("Process Address: %x\n", &test_process1);
	// add_process(process1);

	// process_t* process2 = create_process("process2", &test_process2);
	// printf("Process Address: %x\n", &test_process2);
	// add_process(process2);

	// process_t* process3 = create_process("process3", &test_process3);
	// printf("Process Address: %x\n", &test_process3);
	// add_process(process3);

	print_process_list();
}

void test_elf_loader() {
	load_elf("/user.bin");
	printf("Loaded ELF file\n");
}