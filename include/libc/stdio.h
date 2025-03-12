#pragma once

#include <stdarg.h>
#include <stddef.h>

int vsnprintf(char *buffer, size_t size, const char *format, va_list args);
int snprintf(char *buffer, size_t size, const char *format, ...);