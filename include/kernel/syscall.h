#pragma once

#include <kernel/isr.h>
#include <stdint.h>

#define SYSCALL_WRITE 1
#define SYSCALL_READ 2
#define SYSCALL_OPEN 3
#define SYSCALL_CLOSE 4
#define SYSCALL_EXIT 5
#define SYSCALL_GETDENTS 6

void syscall_handler(registers_t *regs);