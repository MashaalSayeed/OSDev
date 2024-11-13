#pragma once

#include <stdint.h>
#include "kernel/font.h"

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
void draw_image(uint32_t *image_data, uint32_t x, uint32_t y, uint32_t image_width, uint32_t image_height);
void draw_char(char c, uint32_t cx, uint32_t cy, uint32_t fg, uint32_t bg);
void draw_string_at(char *str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
void scroll(uint32_t background_color);