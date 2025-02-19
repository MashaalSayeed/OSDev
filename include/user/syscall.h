#pragma once

#include <stdint.h>

#define SYSCALL_WRITE 1
#define SYSCALL_READ 2
#define SYSCALL_OPEN 3
#define SYSCALL_CLOSE 4
#define SYSCALL_EXIT 5

static inline void syscall(int num, const char* arg1) {
    asm volatile (
        "int $0x80"
        : // No return value
        : "a" (num), "b" (arg1)
        : "memory"
    );
}

int write(int fd, const char *buffer, int size);
int read(int fd, char *buffer, int size);