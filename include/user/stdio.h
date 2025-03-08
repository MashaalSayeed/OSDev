#pragma once

#include <stdint.h>

int stdin = 0;
int stdout = 1;
int stderr = 2;

int printf(const char *format, ...);
int puts(const char *str);
int fgets(char *buffer, int n, int fd);