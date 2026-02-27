#include "user/log.h"
#include "user/syscall.h"
#include "libc/stdio.h"
#include <stdarg.h>

int log_write(const char *s) {
    if (!s) return -1;
    /* write to stdout (fd=1) */
    const char *p = s;
    int len = 0;
    while (p[len]) len++;
    return syscall_write(1, s, len);
}

int log_printf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;
    return syscall_write(1, buf, n);
}
