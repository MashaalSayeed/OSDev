#pragma once

#include <stdint.h>
#include "common/syscall.h"

int printf(const char *format, ...);
int puts(const char *str);
int fgets(char *buffer, int n, int fd);
int getch();