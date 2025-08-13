#pragma once

#include <stdint.h>
#include "kernel/font.h"
#include "kernel/multiboot.h"

typedef struct framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t addr;
} framebuffer_t;

typedef struct {
    int x, y;
    int w, h;
} rect_t;

framebuffer_t framebuffer_from_multiboot(struct multiboot_tag_framebuffer *fb_tag);
void init_framebuffer(framebuffer_t *fb_data);
framebuffer_t * get_framebuffer();

void update_framebuffer();
void update_framebuffer_region(rect_t rect);

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_pixel(uint32_t x, uint32_t y);
void fill_fb(uint32_t color);
void fill_fb_area(rect_t rect, uint32_t color);

// void put_pixel(uint32_t x, uint32_t y, uint32_t color);
// uint32_t get_pixel(uint32_t x, uint32_t y);

// void fill_screen(uint32_t color);
// void draw_image(uint32_t *image_data, uint32_t x, uint32_t y, uint32_t image_width, uint32_t image_height);
// void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
// void draw_char(char c, uint32_t cx, uint32_t cy, uint32_t fg, uint32_t bg);
// void draw_string_at(char *str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
// void scroll(uint32_t background_color);