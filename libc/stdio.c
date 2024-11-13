#include "libc/stdio.h"
#include "drivers/tty.h"
#include "libc/string.h"
#include <stdarg.h>

// Custom printf implementation
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c':
                    terminal_putchar((char)va_arg(args, int));
                    break;
                case 's':
                    terminal_writestring(va_arg(args, char*));
                    break;
                case 'd':
                    int value = va_arg(args, int);
                    char buffer[24];
                    int_to_ascii(value, buffer);
                    terminal_writestring(buffer);
                    break;
                case 'b':
                    int bin_val = va_arg(args, int);
                    char bin_buffer[24];
                    bin_to_ascii(bin_val, bin_buffer);
                    terminal_writestring(bin_buffer);
                    break;
                case 'x':
                    int hex = va_arg(args, int);
                    char hex_buffer[24];
                    hex_to_ascii(hex, hex_buffer);
                    terminal_writestring(hex_buffer);
                    break;
                default:
                    terminal_putchar(*format);
                    break;
            }
        } else {
            terminal_putchar(*format);
        }
        format++;
    }

    va_end(args);
}