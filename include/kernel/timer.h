#pragma once

#include <stdint.h>
#include "isr.h"

void timer_callback(registers_t *regs);
void init_timer(uint32_t freq);