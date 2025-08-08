#include "kernel/framebuffer.h"
#include "kernel/font.h"
#include "kernel/printf.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/string.h"
#include <stddef.h>


psf_font_t *current_font = NULL;
static framebuffer_t *fb;
uint32_t *backbuffer = NULL; // Backbuffer for double buffering

// extern uint16_t *unicode;

framebuffer_t framebuffer_from_multiboot(struct multiboot_tag_framebuffer *fb_tag) {
    framebuffer_t fb_data;
    if (!fb_tag) {
        printf("Error: Framebuffer tag is NULL\n");
        return fb_data; // Return an empty framebuffer
    }

    fb_data.width = fb_tag->framebuffer_width;
    fb_data.height = fb_tag->framebuffer_height;
    fb_data.pitch = fb_tag->framebuffer_pitch;
    fb_data.bpp = fb_tag->framebuffer_bpp;
    fb_data.addr = fb_tag->framebuffer_addr;
    return fb_data;
}

void init_framebuffer(framebuffer_t *fb_data) {
    if (!fb_data) {
        printf("Error: Framebuffer data is NULL\n");
        return;
    }

    if (fb_data->bpp != 32) {
        printf("Error: Unsupported framebuffer bpp: %d\n", fb_data->bpp);
        return;
    }

    fb = fb_data;
    kmap_memory(fb->addr, fb->addr, fb->pitch * fb->height, 0x7);

    // Initialize backbuffer for double buffering
    backbuffer = (uint32_t *)kmalloc(fb_data->width * fb_data->height * (fb_data->bpp / 8));
    if (!backbuffer) {
        printf("Error: Failed to allocate backbuffer memory\n");
        return NULL;
    }

    memset(backbuffer, 0, fb_data->width * fb_data->height * (fb_data->bpp / 8));
}

framebuffer_t * get_framebuffer() {
    return fb;
}

void update_framebuffer() {
    memcpy((void *)fb->addr, backbuffer, fb->width * fb->height * (fb->bpp / 8));
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb->width || y >= fb->height) return;
    backbuffer[y * fb->width + x] = color;
}

uint32_t get_pixel(uint32_t x, uint32_t y) {
    if (x >= fb->width || y >= fb->height) return 0; // Out of bounds
    return backbuffer[y * fb->width + x];
}

void fill_screen(uint32_t color) {
    for (uint32_t y = 0; y < fb->height; ++y) {
        for (uint32_t x = 0; x < fb->width; ++x) {
            put_pixel(x, y, color);
        }
    }
}

// void draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color) {
//     int dx = x2 - x1;
//     int dy = y2 - y1;
//     int steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
//     float x_inc = dx / (float)steps;
//     float y_inc = dy / (float)steps;

//     float x = x1;
//     float y = y1;

//     for (int i = 0; i <= steps; i++) {
//         put_pixel((uint32_t)x, (uint32_t)y, color);
//         x += x_inc;
//         y += y_inc;
//     }
// }

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
    // uint16_t glyph_index = unicode[(uint8_t)c]; // safely look up translation
    // if (glyph_index >= current_font->numglyph) return;
    // c = glyph_index;
    c -= 31;
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
    uint32_t *dest = (uint32_t *)fb->addr;
    uint32_t *src = (uint32_t *)(fb->addr + (fb->pitch * current_font->height));

    // Move all lines up by one font height
    for (uint32_t y = 0; y < fb->height - current_font->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            dest[x] = src[x];
        }
        dest += fb->width;
        src += fb->width;
    }

    // Clear the last line (new blank line at the bottom)
    for (uint32_t x = 0; x < fb->width; x++) {
        put_pixel(x, fb->height - current_font->height, background_color);
    }
}
