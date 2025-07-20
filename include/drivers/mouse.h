#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/isr.h"

#define PS2_MOUSE_DATA_PORT 0x60
#define PS2_MOUSE_CMD_PORT 0x64
#define PS2_MOUSE_ENABLE 0xF4
#define PS2_MOUSE_DISABLE 0xF5
#define PS2_MOUSE_SET_SAMPLE_RATE 0xF3
#define PS2_MOUSE_SET_RESOLUTION 0xE8
#define PS2_MOUSE_SET_SCALING_1TO1 0xE6

#define PS2_MOUSE_ACK 0xFA
#define PS2_MOUSE_NACK 0xFE
#define PS2_MOUSE_ERROR 0xFC

typedef struct {
    int x, y;
    int dx, dy;
    bool left, right, middle;
} mouse_state_t;

typedef struct {
    
} mouse_event_t;

void ps2_mouse_wait(uint8_t type);
void ps2_mouse_write(uint8_t data);
uint8_t ps2_mouse_read();

mouse_state_t* get_mouse_state();
void mouse_irq_handler(registers_t *regs);
void ps2_mouse_init();