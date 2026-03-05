#pragma once

#include <stdint.h>
#include "kernel/isr.h"

uint32_t pit_get_ticks();
void sleep(uint32_t ms);
void pit_init(uint32_t freq);