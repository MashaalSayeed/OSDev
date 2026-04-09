#pragma once

#include <stdint.h>

#define LOAD_MEMORY_ADDRESS 0xC0000000
#define USER_STACK_TOP 0xB0000000
#define SIGRETURN_TRAMPOLINE_ADDR  0xBFFFE000


#define UNUSED(x) (void)(x)
#define ASSERT(cond) do { \
    if (!(cond)) { \
        kprintf(ERROR, "ASSERT FAILED: %s in %s(), at %s:%d\n", #cond, __func__, __FILE__, __LINE__); \
        asm volatile ("cli; hlt"); \
    } \
} while (0)