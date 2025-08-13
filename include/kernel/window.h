#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kernel/gui.h"
#include "kernel/font.h"

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

typedef struct {
    int width, height, pitch;
    uint32_t *pixels;
} surface_t;

typedef struct widget {
    rect_t rect;
    bool visible;

    void (*draw)(struct widget *self, struct window *win);
    void (*on_click)(struct widget *self, int x, int y);

    struct widget *next; // Pointer to the next widget in the linked list
} widget_t;

typedef struct window {
    rect_t rect;
    uint32_t *bitmap;

    const char * title;
    bool focused;
    bool dirty;
    int zindex;

    void (*event_handler)(struct window *win, input_event_t *evt);
    widget_t *widgets; // Linked list of widgets in this window

    struct window *next, *prev; // Doubly linked list pointers
} window_t;

static inline rect_t rect_union(rect_t a, rect_t b) {
    int x1 = MIN(a.x, b.x);
    int y1 = MIN(a.y, b.y);
    int x2 = MAX(a.x + a.w, b.x + b.w);
    int y2 = MAX(a.y + a.h, b.y + b.h);

    return (rect_t){ x1, y1, x2 - x1, y2 - y1 };
}

static inline rect_t rect_intersection(rect_t a, rect_t b) {
    int x1 = MAX(a.x, b.x);
    int y1 = MAX(a.y, b.y);
    int x2 = MIN(a.x + a.w, b.x + b.w);
    int y2 = MIN(a.y + a.h, b.y + b.h);

    if (x1 >= x2 || y1 >= y2) return (rect_t){ 0, 0, 0, 0 }; // No intersection
    return (rect_t){ x1, y1, x2 - x1, y2 - y1 };
}

static inline int rects_overlap(rect_t a, rect_t b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
             a.y + a.h <= b.y || b.y + b.h <= a.y);
}

static inline rect_t rect_subtract(rect_t a, rect_t b) {
    if (!rects_overlap(a, b)) return a; // No overlap, return original rect

    int x1 = MAX(a.x, b.x);
    int y1 = MAX(a.y, b.y);
    int x2 = MIN(a.x + a.w, b.x + b.w);
    int y2 = MIN(a.y + a.h, b.y + b.h);

    return (rect_t){ x1, y1, x2 - x1, y2 - y1 };
}

window_t *create_window(int x, int y, int width, int height, const char *title);
void destroy_window(window_t *window);

void draw_pixel(window_t *win, int x, int y, uint32_t color);
void draw_rect(window_t *win, int x, int y, int width, int height, uint32_t color);
void fill_window(window_t *win, uint32_t color);

int is_pixel_set(unsigned char *glyph, int x);
void draw_char(window_t *win, char c, int x, int y, uint32_t fg, uint32_t bg, psf_font_t *font);
void draw_string(window_t *win, const char *str, int x, int y, uint32_t fg, uint32_t bg, psf_font_t *font);
void draw_titlebar(window_t *win, psf_font_t *font);
void draw_window_border(window_t *win, uint32_t color);
void draw_window_to_fb(window_t *win);