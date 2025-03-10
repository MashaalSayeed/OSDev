#include "kernel/tests.h"
#include "kernel/kheap.h"
#include "kernel/process.h"
#include "kernel/locks.h"
#include "kernel/printf.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "drivers/pit.h"
#include "system.h"
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

static void fgets(int fd, char* buffer, int size) {
	asm volatile (
		"int $0x80"             // Trigger system call
		: "=a" (size)         // Return value in eax
		: "a" (2),      // Syscall number in eax
		  "b" (fd),             // File descriptor in ebx
		  "c" (buffer),         // Buffer address in ecx
		  "d" (size)            // Size in edx
		: "memory"
	);
}

static int ufork() {
	int pid;
	asm volatile (
		"int $0x80"
		: "=a" (pid)
		: "a" (7)
		: "memory"
	);
	uprintf("Forked process with PID: %d\n", pid);
	return pid;
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

spinlock_t lock;
static void test_process1() {
	int a = 0;
    while (1) {
		uprintf("Hello from process 1!\n");
		sleep(10);
    }
}

static void test_process2() {
	while (1) {
		uprintf("Hello from process 2!\n");
		sleep(10);
	}
}

static void exit(int arg) {
	asm volatile (
		"int $0x80"
		:
		: "a"(5), "b"(arg)
		: "memory"
	);
}

static void shell() {
	while (1) {
		char buffer[80];
		uprintf("%x> ", buffer);
		fgets(0, buffer, 80);
		uprintf("You entered: %s\n", buffer);

		if (strcmp(buffer, "exit") == 0) {
			break;
		}
	}

	exit(0);
}

void fork_proc() {
	int my_pid = getpid();
    uprintf("[PID %d] Starting test_fork()\n", my_pid);

	int pid = ufork();
	if (pid == 0) {
		my_pid = getpid();
        uprintf("[PID %d] Hello from child process!\n", my_pid);
		sleep(200);
	} else if (pid > 0) {
        uprintf("[PID %d] Hello from parent process! Child PID: %d\n", my_pid, pid);
		sleep(200);
	} else {
        uprintf("[PID %d] Fork failed\n", my_pid);
	}

	uprintf("[PID %d] Exiting test_fork()\n", my_pid);
	exit(0);
}

void test_scheduler() {
	process_t* process1 = create_process("process1", &test_process1);
	add_process(process1);

	process_t* process2 = create_process("process2", &test_process2);
	add_process(process2);

	print_process_list();
}

void test_fork() {
	process_t* process3 = create_process("fork", &fork_proc);
	add_process(process3);
}

void test_shell() {
	process_t* process = create_process("shell", &shell);
	add_process(process);
}