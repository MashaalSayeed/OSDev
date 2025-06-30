#include "user/syscall.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include <stdarg.h>

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[80];
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (count > 0) return syscall_write(STDOUT, buffer, count);
    return -1;
}

int puts(const char *str) {
    if (!str) return -1;

    size_t len = strlen(str);
    if (syscall_write(STDOUT, str, len) < 0) return -1;

    char newline = '\n';
    if (syscall_write(STDOUT, &newline, 1) < 0) return -1;

    return len + 1;
}

int fgets(char *buffer, int n, int fd) {
    if (n <= 0) return -1;
    int i = 0;
    char c = 0;
    while (i < n - 1) {
        if (syscall_read(fd, &c, 1) <= 0) break;

        if (fd == STDIN) syscall_write(STDOUT, &c, 1); // Echo input

        buffer[i++] = c;
        if (c == '\n') break;
    }
    buffer[i] = '\0';
    return i;
}

int getch() {
    char c;
    if (syscall_read(STDIN, &c, 1) < 0) return -1;
    return c;
}