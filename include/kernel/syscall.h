#pragma once

#include <kernel/isr.h>
#include <stdint.h>
#include <common/syscall.h>

#define EINVAL 22

typedef int (*syscall_t)(uint32_t, uint32_t, uint32_t);

void syscall_handler(registers_t *regs);