#include "kernel/tests.h"
#include "kernel/kheap.h"
#include "kernel/process.h"
#include "kernel/locks.h"
#include "kernel/printf.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "drivers/pit.h"
#include "kernel/system.h"
#include "common/syscall.h"
#include <stdarg.h>

void test_divide_by_zero() {
	int x = 1;
	int y = 0;
	int z = x / y;
	UNUSED(z);
}

void test_heap() {
	print_kheap();
	uint8_t* ptr1 = (uint8_t *)kmalloc(0x8);
	uint8_t* ptr2 = (uint8_t *)kmalloc(0x8);

	printf("Allocated memory at %x\n", ptr1);
	printf("Allocated memory at %x\n", ptr2);

	kfree(ptr2);
	printf("Freed memory at %x\n", ptr2);

	ptr2 = kmalloc(0x20);
	printf("Allocated memory at %x\n\n", ptr2);

	kfree(ptr1);
	kfree(ptr2);
	print_kheap();
}

void test_printf() {
	printf("Hello, %s!\n", "world");
	printf("Number: %d\n", -42);
	printf("Hex: %x\n", 0x1234);
	printf("Binary: %b\n", 0b1010);
}

void test_string() {
	char str1[20] = "Hello, ";
	char str2[] = "world!";
	strcat(str1, str2);
	printf("Concatenated string: %s\n", str1);

	char str3[20];
	strcpy(str3, str1);
	printf("Copied string: %s\n", str3);

	int cmp_result = strcmp(str1, str3);
	printf("String comparison result: %d\n", cmp_result);

	int row, col;
	sscanf("10;20", "%d;%d", &row, &col);
	printf("Parsed values: row=%d, col=%d\n", row, col);
}

static void uprintf(const char* format, ...) {
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

static void do_syscall(int syscall_number, int arg1, int arg2, int arg3) {
	asm volatile (
		"int $0x80"
		:
		: "a"(syscall_number), "b"(arg1), "c"(arg2), "d"(arg3)
		: "memory"
	);
}

static int getpid() {
	int pid;
	asm volatile (
		"int $0x80"
		: "=a" (pid)
		: "a" (10)
		: "memory"
	);
	return pid;
}

static void test_process() {
	int pid = getpid();
	while (1) {
		uprintf("Hello from process %d!\n", pid);
		sleep(10);
	}
}

static void shell() {
	while (1) {
		char buffer[80];
		uprintf("%x> ", buffer);
		do_syscall(SYSCALL_READ, 0, (int)buffer, 80); // fgets
		uprintf("You entered: %s\n", buffer);

		if (strcmp(buffer, "exit") == 0) {
			break;
		}
	}

	do_syscall(SYSCALL_EXIT, 0, 0, 0);
}

void fork_proc() {
	int my_pid = getpid();
    uprintf("[PID %d] Starting test_fork()\n", my_pid);

	int pid;
	asm volatile ("int $0x80" : "=a" (pid) : "a" (7) : "memory"); // fork
	if (pid == 0) {
		my_pid = getpid();
        uprintf("[PID %d] Hello from child process!\n", my_pid);
		sleep(50);
	} else if (pid > 0) {
        uprintf("[PID %d] Hello from parent process! Child PID: %d\n", my_pid, pid);
		do_syscall(9, pid, 0, 0);
	} else {
        uprintf("[PID %d] Fork failed\n", my_pid);
	}

	uprintf("[PID %d] Exiting test_fork()\n", my_pid);
	do_syscall(SYSCALL_EXIT, 0, 0, 0);
}

void test_scheduler() {
	process_t* process1 = create_process("process1", &test_process, PROCESS_FLAG_USER);
	process_t* process2 = create_process("process2", &test_process, PROCESS_FLAG_USER);
	process_t* process3 = create_process("process3", &test_process, PROCESS_FLAG_USER);

	add_process(process1);
	add_process(process2);
	add_process(process3);
	// print_process_list();
	print_thread_list();
}

void test_fork() {
	process_t* process3 = create_process("fork", &fork_proc, PROCESS_FLAG_USER);
	add_process(process3);
}

void test_shell() {
	process_t* process = create_process("shell", &shell, PROCESS_FLAG_USER);
	add_process(process);
}