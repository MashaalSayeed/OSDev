#pragma once

#include <kernel/isr.h>
#include <stdint.h>
#include <syscall.h>

void syscall_handler(registers_t *regs);