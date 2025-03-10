#pragma once

#include <stdint.h>
#include <stddef.h>
#include <syscall.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 64

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
int syscall_getpid();
void *syscall_sbrk(int increment);
int syscall_mkdir(const char *path, int mode);
int syscall_rmdir(const char *path);
int syscall_unlink(const char *path);