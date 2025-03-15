#include "user/syscall.h"
#include "libc/stdio.h"
#include <stdarg.h>

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[80];
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return syscall_write(1, buffer, count);
}

int puts(const char *str) {
    return syscall_write(1, str, 0);
}

int fgets(char *buffer, int n, int fd) {
    if (n <= 0) return -1;
    int i = 0;
    char c = 0;
    while (i < n - 1) {
        if (syscall_read(fd, &c, 1) <= 0) break;

        if (fd == 0) syscall_write(1, &c, 1); // Echo input

        buffer[i++] = c;
        if (c == '\n') break;
    }
    buffer[i] = '\0';
    return i;
}