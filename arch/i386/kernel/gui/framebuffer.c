#include "kernel/framebuffer.h"
#include "kernel/font.h"
#include <stddef.h>

framebuffer_t fb;

psf_font_t *current_font = NULL;

framebuffer_t * init_framebuffer(uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp, uint32_t addr) {
    fb.width = width;
    fb.height = height;
    fb.pitch = pitch;
    fb.bpp = bpp;
    fb.addr = addr;

    return &fb;
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

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; ++i) {
        for (uint32_t j = 0; j < w; ++j) {
            put_pixel(x + j, y + i, color);
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

int is_pixel_set(unsigned char *glyph, int x) {
    return glyph[x / 8] & (0x80 >> (x % 8));
}

void draw_char(char c, uint32_t cx, uint32_t cy, uint32_t fg, uint32_t bg) {
    if ((uint32_t)c >= current_font->numglyph) return;

    int bytesperline = (current_font->width + 7) / 8;
    unsigned char *glyph = (unsigned char *)current_font + current_font->headersize + (c * current_font->bytesperglyph);

    for (uint32_t y = 0; y < current_font->height; y++) {
        for (uint32_t x = 0; x < current_font->width; x++) {
            put_pixel(cx + x, cy + y, is_pixel_set(glyph, x) ? fg : bg);
        }
        glyph += bytesperline;
    }
}

void draw_string_at(char *str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    uint32_t cx = x;
    uint32_t cy = y;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += current_font->height;
        } else {
            draw_char(*str, cx, cy, fg, bg);
            cx += current_font->width;
        }
        str++;
    }
}

void scroll(uint32_t background_color) {
    uint32_t *dest = (uint32_t *)fb.addr;
    uint32_t *src = (uint32_t *)(fb.addr + (fb.pitch * current_font->height));

    // Move all lines up by one font height
    for (uint32_t y = 0; y < fb.height - current_font->height; y++) {
        for (uint32_t x = 0; x < fb.width; x++) {
            dest[x] = src[x];
        }
        dest += fb.width;
        src += fb.width;
    }

    // Clear the last line (new blank line at the bottom)
    for (uint32_t x = 0; x < fb.width; x++) {
        put_pixel(x, fb.height - current_font->height, background_color);
    }
}
