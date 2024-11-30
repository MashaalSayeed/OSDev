#include "kernel/tests.h"
#include "kernel/kheap.h"
#include "kernel/process.h"
#include "kernel/timer.h"
#include "kernel/locks.h"
#include "system.h"
#include "libc/stdio.h"

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
    while (1) {
		spinlock_acquire(&lock);
        printf("Process 1: ESP = %x\n", get_esp());
		sleep(100);
		spinlock_release(&lock);
    }
}

void test_process2() {
	while (1) {
		spinlock_acquire(&lock);
		printf("Process 2: ESP = %x\n", get_esp());
		spinlock_release(&lock);
	}
}

void test_scheduler() {
	initialize_lock(&lock);

	process_t* process1 = create_process("process1", &test_process1);
	printf("Process Address: %x\n", &test_process1);
	add_process(process1);

	// Dont uncomment this.. it will cause a kernel panic (Race condition on terminal)
	process_t* process2 = create_process("process2", &test_process2);
	printf("Process Address: %x\n", &test_process2);
	add_process(process2);

	print_process_list();
}