#pragma once

#include <stdint.h>
#include "kernel/isr.h"

void timer_callback(registers_t *regs);
uint32_t get_timer_ticks();
void sleep(uint32_t ms);
void init_timer(uint32_t freq);