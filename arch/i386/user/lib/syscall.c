#include "user/syscall.h"

int syscall_write(int fd, const char *buffer, size_t size)
{
    int result;
    asm volatile(
        "int $0x80"           // Trigger system call
        : "=a"(result)        // Return value in eax
        : "a"(SYSCALL_WRITE), // Syscall number in eax
          "b"(fd),            // File descriptor in ebx
          "c"(buffer),        // Buffer address in ecx
          "d"(size)           // Size in edx
        : "memory");

    return result; // Return number of bytes written or -1 on error
}

int syscall_read(int fd, char *buffer, size_t size)
{
    int result;
    asm volatile(
        "int $0x80"          // Trigger system call
        : "=a"(result)       // Return value in eax
        : "a"(SYSCALL_READ), // Syscall number in eax
          "b"(fd),           // File descriptor in ebx
          "c"(buffer),       // Buffer address in ecx
          "d"(size)          // Size in edx
        : "memory");

    return result; // Return number of bytes read or -1 on error
}

int syscall_open(const char *path, int flags)
{
    int result;
    asm volatile(
        "int $0x80"          // Trigger system call
        : "=a"(result)       // Return value in eax
        : "a"(SYSCALL_OPEN), // Syscall number in eax
          "b"(path),         // Path in ebx
          "c"(flags)         // Flags in ecx
        : "memory");

    return result; // Return file descriptor or -1 on error
}

int syscall_close(int fd)
{
    int result;
    asm volatile(
        "int $0x80"           // Trigger system call
        : "=a"(result)        // Return value in eax
        : "a"(SYSCALL_CLOSE), // Syscall number in eax
          "b"(fd)             // File descriptor in ebx
        : "memory");

    return result; // Return 0 on success or -1 on error
}

void syscall_exit(int status)
{
    asm volatile(
        "int $0x80" // Trigger system call
        :
        : "a"(SYSCALL_EXIT), // Syscall number in eax
          "b"(status)        // Status code in ebx
        : "memory");
}

int syscall_getdents(int fd, void *dirp, size_t count)
{
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GETDENTS),
          "b"(fd),
          "c"(dirp),
          "d"(count)
        : "memory");
    return result;
}

int syscall_getpid()
{
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GETPID)
        : "memory");
    return result;
}

void *syscall_sbrk(int incr)
{
    void *result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SBRK),
          "b"(incr)
        : "memory");
    return result;
}

int syscall_mkdir(const char *path, int mode)
{
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_MKDIR),
          "b"(path),
          "c"(mode)
        : "memory");
    return result;
}

int syscall_rmdir(const char *path)
{
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_RMDIR),
          "b"(path)
        : "memory");
    return result;
}

int syscall_unlink(const char *path)
{
    int result;
    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_UNLINK),
          "b"(path)
        : "memory");
    return result;
}