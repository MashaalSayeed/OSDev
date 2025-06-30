#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct window {
    int x, y;
    int width, height;
    const char * title;
    uint32_t bg_color;
    bool focused;

    struct window *next; // Pointer to the next window in the linked list
} window_t;

window_t *create_window(int x, int y, int width, int height, const char *title, uint32_t bg_color);
void destroy_window(window_t *window);
void draw_window(window_t *window);