#include "user/syscall.h"


int write(int fd, const char *buffer, int size) {
    int result;
    asm volatile (
        "int $0x80"             // Trigger system call
        : "=a" (result)         // Return value in eax
        : "a" (SYSCALL_WRITE),      // Syscall number in eax
          "b" (fd),             // File descriptor in ebx
          "c" (buffer),         // Buffer address in ecx
          "d" (size)            // Size in edx
        : "memory"
    );

    return result;  // Return number of bytes written or -1 on error
}


int read(int fd, char *buffer, int size) {
    int result;
    asm volatile (
        "int $0x80"             // Trigger system call
        : "=a" (result)         // Return value in eax
        : "a" (SYSCALL_READ),      // Syscall number in eax
          "b" (fd),             // File descriptor in ebx
          "c" (buffer),         // Buffer address in ecx
          "d" (size)            // Size in edx
        : "memory"
    );

    return result;  // Return number of bytes read or -1 on error
}
