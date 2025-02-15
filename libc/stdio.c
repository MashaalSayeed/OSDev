#include <stdarg.h>
#include <stdbool.h>
#include "libc/string.h"
#include "libc/stdio.h"

void str_append(char *buffer, size_t size, size_t *i, const char *str) {
    while (*str && *i < size - 1) {
        buffer[*i] = *str;
        (*i)++;
        str++;
    }
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args) {
    size_t i = 0;
    char num_buffer[24];

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    if (i < size - 1) buffer[i] = c;
                    i++;
                    break;
                }
                case 's': {
                    const char *str = va_arg(args, char*);
                    if (str) str_append(buffer, size, &i, str);
                    break;
                }
                case 'd': {
                    int_to_ascii(va_arg(args, int), num_buffer);
                    str_append(buffer, size, &i, num_buffer);
                    break;
                }
                case 'b': {
                    bin_to_ascii(va_arg(args, int), num_buffer);
                    str_append(buffer, size, &i, num_buffer);
                    break;
                }
                case 'x': {
                    hex_to_ascii(va_arg(args, int), num_buffer);
                    str_append(buffer, size, &i, num_buffer);
                    break;
                }
                default:
                    if (i < size - 1) buffer[i] = *format;
                    i++;
                    break;
            }
        } else {
            if (i < size - 1) buffer[i] = *format;
            i++;
        }
        format++;
    }

    if (size > 0) buffer[i < size ? i : size - 1] = '\0';
    return i;
}