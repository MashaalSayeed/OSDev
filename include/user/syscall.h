#pragma once

#include <stdint.h>

#define SYSCALL_WRITE 0

static inline void syscall(int num, int arg1) {
    asm volatile (
        "int $0x80"
        : // No return value
        : "a" (num), "b" (arg1)
        : "memory"
    );
}

static inline void write(int fd, const char *buffer, int size) {
    asm volatile (
        "movl $1, %%eax;"  // Syscall number for write (example)
        "movl %0, %%ebx;"  // File descriptor
        "movl %1, %%ecx;"  // Buffer pointer
        "movl %2, %%edx;"  // Size
        "int $0x80;"       // Trigger interrupt for syscall
        :
        : "g"(fd), "g"(buffer), "g"(size)
        : "eax", "ebx", "ecx", "edx"
    );
}