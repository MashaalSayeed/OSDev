#pragma once

#include <stdint.h>
#define stdin 0
#define stdout 1
#define stderr 2

int printf(const char *format, ...);
int puts(const char *str);
int fgets(char *buffer, int n, int fd);