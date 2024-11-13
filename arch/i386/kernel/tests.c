#include "kernel/tests.h"
#include "system.h"

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
