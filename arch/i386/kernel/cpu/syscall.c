#include "kernel/syscall.h"

// TODO: Move outside of kernel
static inline void syscall(int num, int arg1) {
    asm volatile (
        "int $0x80"
        : // No return value
        : "a" (num), "b" (arg1)
        : "memory"
    );
}

// printf wrapper for user space
void uprintf(const char *msg) {
    syscall(SYSCALL_WRITE, (int)msg);
}