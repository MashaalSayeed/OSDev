#include "libc/stdio.h"
#include "user/syscall.h"

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[1024];
    int ret = vsnprintf(buffer, 1024, format, args);
    va_end(args);

    // write(1, buffer, ret);
    syscall(0, (int)buffer);
    return ret;
}