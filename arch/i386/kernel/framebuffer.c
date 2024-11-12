#include "kernel/framebuffer.h"

framebuffer_t fb;

void init_framebuffer(uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp, uint32_t addr) {
    fb.width = width;
    fb.height = height;
    fb.pitch = pitch;
    fb.bpp = bpp;
    fb.addr = addr;
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    uint32_t *pixel = (uint32_t*)(fb.addr + (y * fb.pitch) + (x * (fb.bpp / 8)));
    *pixel = color;
}

void fill_screen(uint32_t color) {
    for (uint32_t y = 0; y < fb.height; ++y) {
        for (uint32_t x = 0; x < fb.width; ++x) {
            put_pixel(x, y, color);
        }
    }
}

void draw_image(uint32_t *image_data, uint32_t x, uint32_t y, uint32_t image_width, uint32_t image_height) {
    for (uint32_t row = 0; row < image_height; row++) {
        for (uint32_t col = 0; col < image_width; col++) {
            uint32_t pixel_color = image_data[row * image_width + col];
            put_pixel(x + col, y + row, pixel_color);
        }
    }
}