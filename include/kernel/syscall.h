#pragma once

#include <stdint.h>

#define SYSCALL_WRITE 0

static inline void syscall(int num, int arg1);
void uprintf(const char *msg);