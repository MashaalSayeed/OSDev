#pragma once

#include <stdint.h>
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