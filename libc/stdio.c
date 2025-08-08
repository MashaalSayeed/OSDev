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

    if (!buffer || !format || size == 0) return 0;

    while (*format) {
        if (*format == '%') {
            format++;
            if (!*format) break;

            int width = 0;
            bool zero_pad = false;

            if (*format == '0') {
                zero_pad = true;
                format++;
            }

            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }
            
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
                    int len = strlen(num_buffer);
                    while (len < width) {
                        if (i < size - 1) {
                            buffer[i] = zero_pad ? '0' : ' ';
                            i++;
                            len++;
                        } else {
                            break;
                        }
                    }

                    str_append(buffer, size, &i, num_buffer);
                    break;
                }
                case 'b': {
                    bin_to_ascii(va_arg(args, unsigned int), num_buffer);
                    str_append(buffer, size, &i, num_buffer);
                    break;
                }
                case 'x': {
                    hex_to_ascii(va_arg(args, unsigned int), num_buffer);
                    int len = strlen(num_buffer);
                    while (len < width) {
                        if (i < size - 1) {
                            buffer[i] = zero_pad ? '0' : ' ';
                            i++;
                            len++;
                        } else {
                            break;
                        }
                    }
                    
                    str_append(buffer, size, &i, num_buffer);
                    break;
                }
                default:
                    if (i < size - 1) buffer[i++] = '%';
                    if (i < size - 1) buffer[i++] = *format;
                    break;
            }
        } else {
            if (i < size - 1) buffer[i++] = *format;
        }
        format++;
    }
    
    buffer[i < size ? i : size - 1] = '\0';
    return i;
}

int snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, size, format, args);
    va_end(args);
    return ret;
}

static const char* parse_int(const char *str, int *value) {
    int val = 0, sign = 1;
    while (*str == ' ' || *str == '\t') str++; // Skip whitespace
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    if (*str < '0' || *str > '9') return NULL;

    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str - '0');
        str++;
    }
    
    *value = val * sign;
    return str;
}

int vsscanf(const char *str, const char *format, va_list args) {
    int ret = 0;

    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 'd') {
                int *val = va_arg(args, int*);
                str = parse_int(str, val);
                if (!str) break; // If parsing failed, exit

                ret++;
            } else if (*format == 's') {
                char *out = va_arg(args, char*);
                while (*str == ' ' || *str == '\t' || *str == '\n') str++;
                while (*str && *str != ' ' && *str != '\t' && *str != '\n') {
                    *out++ = *str++;
                }

                *out = '\0'; // Null-terminate the string
                ret++;
            }
 
            format++; // Move past the format specifier
        } else {
            if (*str == *format) {
                str++;
                format++;
            } else {
                break;
            }
        }
    }

    return ret;
}

int sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsscanf(str, format, args);
    va_end(args);
    return ret;
}