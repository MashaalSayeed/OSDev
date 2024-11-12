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