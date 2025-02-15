#include <stdbool.h>
#include "libc/stdio.h"
#include "kernel/framebuffer.h"
#include "drivers/tty.h"


bool is_vbe_enabled = false;
void puts(const char* str) {
    if (is_vbe_enabled) {
    } else {
        terminal_writestring(str);
    }
}

void putchar(char c) {
    if (is_vbe_enabled) {

    } else {
        terminal_putchar(c);
    }
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    int ret = vsnprintf(buffer, sizeof(buffer), format, args);
    puts(buffer);
    va_end(args);
    return ret;
}