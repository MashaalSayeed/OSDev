#pragma once 

#include "user/stdio.h"

/* Write a NUL-terminated string to the process stdout (fd=1). */
int log_write(const char *s);

/* printf-style logging to stdout. */
int log_printf(const char *fmt, ...);