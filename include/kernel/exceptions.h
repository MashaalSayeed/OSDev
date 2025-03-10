#pragma once

#include <stdint.h>
#include <kernel/isr.h>

void print_debug_info(registers_t *regs);
void print_stack_trace(registers_t *regs);
void exceptions_install();