#pragma once

#include <stdint.h>
#include <stddef.h>

#define SYSCALL_WRITE 1
#define SYSCALL_READ 2
#define SYSCALL_OPEN 3
#define SYSCALL_CLOSE 4
#define SYSCALL_EXIT 5
#define SYSCALL_GETDENTS 6

static inline void syscall(int num, const char* arg1) {
    asm volatile (
        "int $0x80"
        : // No return value
        : "a" (num), "b" (arg1)
        : "memory"
    );
}

int syscall_write(int fd, const char *buffer, size_t size);
int syscall_read(int fd, char *buffer, size_t size);
int syscall_open(const char *path, int flags);
int syscall_close(int fd);
void syscall_exit(int status);
int syscall_getdents(int fd, void *dirp, size_t count);