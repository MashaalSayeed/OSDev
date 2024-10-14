#pragma once

#include <stdint.h>
#include "isr.h"

void keyboard_callback(registers_t *regs);
void init_keyboard();