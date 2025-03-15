#pragma once

#include <kernel/isr.h>
#include <stdint.h>
#include <syscall.h>

typedef int (*syscall_t)(uint32_t, uint32_t, uint32_t);

void syscall_handler(registers_t *regs);