#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum { DEBUG, INFO, WARNING, ERROR } log_level_t;

int printf(const char *format, ...);
void puts(const char* str);
void putchar(char c);
void hexdump(const void *addr, size_t len);
