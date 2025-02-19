#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kernel/isr.h"

void keyboard_callback(registers_t *regs);
char kgetch();
char * kgets(char *buffer, size_t size);
void init_keyboard();