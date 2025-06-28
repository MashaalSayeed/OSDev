#pragma once

#include <stdint.h>
#include <stddef.h>

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

void ps2_enable_mouse();
void ps2_disable_mouse();
void ps2_set_mouse_sample_rate(uint8_t rate);
void ps2_set_mouse_resolution(uint8_t resolution);
void ps2_set_mouse_scaling(uint8_t scaling);