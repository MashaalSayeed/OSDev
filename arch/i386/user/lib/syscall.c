#include "user/syscall.h"
#include "user/syscall.h"

static inline int syscall(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "b" (arg1), "c" (arg2), "d" (arg3)
        : "memory"
    );
    return result;
}

int syscall_write(int fd, const char *buffer, size_t size) {
    return syscall(SYSCALL_WRITE, fd, (int)buffer, size);
}

int syscall_read(int fd, char *buffer, size_t size) {
    return syscall(SYSCALL_READ, fd, (int)buffer, size);
}

int syscall_open(const char *path, int flags) {
    return syscall(SYSCALL_OPEN, (int)path, flags, 0);
}

int syscall_close(int fd) {
    return syscall(SYSCALL_CLOSE, fd, 0, 0);
}

int syscall_lseek(int fd, uint32_t offset, int whence) {
    return syscall(SYSCALL_LSEEK, fd, offset, whence);
}

void syscall_exit(int status) {
    syscall(SYSCALL_EXIT, status, 0, 0);
}

int syscall_getdents(int fd, void *dirp, size_t count) {
    return syscall(SYSCALL_GETDENTS, fd, (int)dirp, count);
}

int syscall_fork() {
    return syscall(SYSCALL_FORK, 0, 0, 0);
}

int syscall_exec(const char *path, char **args) {
    return syscall(SYSCALL_EXEC, (uintptr_t)path, (uintptr_t)args, 0);
}

int syscall_dup2(int oldfd, int newfd) {
    return syscall(SYSCALL_DUP2, oldfd, newfd, 0);
}

int syscall_waitpid(int pid, int *status, int options) {
    return syscall(SYSCALL_WAITPID, pid, (int)status, options);
}

int syscall_getpid() {
    return syscall(SYSCALL_GETPID, 0, 0, 0);
}

void *syscall_sbrk(int incr) {
    return (void *)syscall(SYSCALL_SBRK, incr, 0, 0);
}

char *syscall_getcwd(char *buf, size_t size) {
    return (char *)syscall(SYSCALL_GETCWD, (int)buf, size, 0);
}

int syscall_chdir(const char *path) {
    return syscall(SYSCALL_CHDIR, (int)path, 0, 0);
}

int syscall_mkdir(const char *path, int mode) {
    return syscall(SYSCALL_MKDIR, (int)path, mode, 0);
}

int syscall_rmdir(const char *path) {
    return syscall(SYSCALL_RMDIR, (int)path, 0, 0);
}

int syscall_unlink(const char *path) {
    return syscall(SYSCALL_UNLINK, (int)path, 0, 0);
}