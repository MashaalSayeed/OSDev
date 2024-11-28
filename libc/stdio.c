#include "kernel/framebuffer.h"
#include "libc/stdio.h"
#include "libc/string.h"
#include "drivers/tty.h"
#include <stdarg.h>
#include <stdbool.h>

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

// Custom printf implementation
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c':
                    putchar((char)va_arg(args, int));
                    break;
                case 's':
                    puts(va_arg(args, char*));
                    break;
                case 'd':
                    int value = va_arg(args, int);
                    char buffer[24];
                    int_to_ascii(value, buffer);
                    puts(buffer);
                    break;
                case 'b':
                    int bin_val = va_arg(args, int);
                    char bin_buffer[24];
                    bin_to_ascii(bin_val, bin_buffer);
                    puts(bin_buffer);
                    break;
                case 'x':
                    int hex = va_arg(args, int);
                    char hex_buffer[24];
                    hex_to_ascii(hex, hex_buffer);
                    puts(hex_buffer);
                    break;
                default:
                    putchar(*format);
                    break;
            }
        } else {
            putchar(*format);
        }
        format++;
    }

    va_end(args);
}