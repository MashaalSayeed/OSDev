#include "user/syscall.h"
#include "libc/stdio.h"
#include <stdarg.h>

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[80];
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return syscall_write(1, buffer, 0);
}

int puts(const char *str) {
    return syscall_write(1, str, 0);
}

int fgets(char *buffer, int n, int fd) {
    return syscall_read(fd, buffer, n);
}