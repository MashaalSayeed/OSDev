#pragma once

#include <stdint.h>
#include "kernel/isr.h"

void keyboard_callback(registers_t *regs);
void init_keyboard();