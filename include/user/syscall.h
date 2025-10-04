#pragma once

#include <stdint.h>
#include <stddef.h>
#include <common/syscall.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_APPEND 8
#define O_TRUNC 32
#define O_CREAT 64

int syscall_write(int fd, const char *buffer, size_t size);
int syscall_read(int fd, char *buffer, size_t size);
int syscall_open(const char *path, int flags);
int syscall_close(int fd);
void syscall_exit(int status);
int syscall_getdents(int fd, void *dirp, size_t count);
int syscall_fork();
int syscall_exec(const char *path, char **args);
int syscall_dup2(int oldfd, int newfd);
char *syscall_getcwd(char *buf, size_t size);
int syscall_chdir(const char *path);
int syscall_waitpid(int pid, int *status, int options);
int syscall_getpid();
void *syscall_sbrk(int increment);
int syscall_mkdir(const char *path, int mode);
int syscall_rmdir(const char *path);
int syscall_unlink(const char *path);
int syscall_pipe(int fds[2]);