#include "kernel/window.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "kernel/framebuffer.h"

window_t * create_window(int x, int y, int width, int height, const char *title, uint32_t bg_color) {
    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) {
        printf("Error: Failed to allocate memory for window\n");
        return NULL;
    }

    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->title = title;
    win->bg_color = bg_color;
    win->focused = false;
    win->next = NULL;

    return win;
}

void destroy_window(window_t *win) {
    if (!win) return;

    // Free the window structure
    kfree(win);
}

void draw_window(window_t *win) {
    if (!win) return;

    // Draw the window border
    draw_rect(win->x, win->y, win->width, win->height, win->bg_color);
    draw_rect(win->x, win->y, win->width, 20, 0xCCCCCC); // Title bar

    // Draw the title
    draw_string_at((char *)win->title, win->x + 10, win->y + 3, 0x000000, 0xCCCCCC);
}