#include "kernel/framebuffer.h"
#include "kernel/printf.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/string.h"
#include <stddef.h>


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
    printf("Framebuffer initialized: %dx%d, pitch: %d, bpp: %d\n", fb->width, fb->height, fb->pitch, fb->bpp);
}


// void scroll(uint32_t background_color) {
//     uint32_t *dest = (uint32_t *)fb->addr;
//     uint32_t *src = (uint32_t *)(fb->addr + (fb->pitch * current_font->height));

//     // Move all lines up by one font height
//     for (uint32_t y = 0; y < fb->height - current_font->height; y++) {
//         for (uint32_t x = 0; x < fb->width; x++) {
//             dest[x] = src[x];
//         }
//         dest += fb->width;
//         src += fb->width;
//     }

//     // Clear the last line (new blank line at the bottom)
//     for (uint32_t x = 0; x < fb->width; x++) {
//         put_pixel(x, fb->height - current_font->height, background_color);
//     }
// }
