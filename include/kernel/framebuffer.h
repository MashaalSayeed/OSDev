#pragma once

#include <stdint.h>

typedef struct framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t addr;
} framebuffer_t;

void init_framebuffer(uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp, uint32_t addr);
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fill_screen(uint32_t color);