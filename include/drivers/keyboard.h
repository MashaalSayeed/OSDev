#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kernel/isr.h"

void keyboard_callback(registers_t *regs);
char kgetch();
int  keyboard_try_getchar(void); /* non-blocking; returns -1 if empty */
int kgets(char *buffer, size_t size);
size_t read_keyboard_buffer(char *buf, size_t size);
void init_keyboard();