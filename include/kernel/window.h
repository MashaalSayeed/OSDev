#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "kernel/gui.h"
#include "kernel/font.h"

#define BORDER_WIDTH 1
#define TITLEBAR_HEIGHT 20

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

typedef struct {
    int width, height, pitch;
    uint32_t *pixels;
} surface_t;

typedef enum {
    WIDGET_LABEL,
    WIDGET_BUTTON,
    WIDGET_IMAGE
} widget_type_t;

typedef struct {
    const char *text;
    uint32_t color;
} label_data_t;

typedef struct {
    const char *text;
    uint32_t color;
} button_data_t;

typedef struct {
    const char *image_path;
} image_data_t;

typedef struct widget {
    widget_type_t type;
    rect_t rect;
    bool visible;
    void *data; // For custom data, e.g. button state

    void (*draw)(struct widget *self, surface_t *win);
    void (*on_click)(struct widget *self, int x, int y); // Click handler for buttons

    struct widget *next; // Pointer to the next widget in the linked list
} widget_t;

typedef enum {
    WINDOW_TYPE_NORMAL,
    WINDOW_TYPE_DIALOG,
    WINDOW_TYPE_POPUP
} window_type_t;

typedef struct window {
    rect_t rect;
    surface_t surface;
    rect_t content;

    const char * title;
    bool focused;
    bool dirty;
    int zindex;

    void (*event_handler)(struct window *win, input_event_t *evt);
    widget_t *widgets; // Linked list of widgets in this window

    struct window *next, *prev; // Doubly linked list pointers
} window_t;

static inline int clamp(int value, int min, int max) {
    return value < min ? min : (value > max ? max : value);
}

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

}

static inline void surf_put(surface_t *surf, int x, int y, uint32_t color) {
    if (x >= surf->width || y >= surf->height) return;
    surf->pixels[y * surf->pitch + x] = color;
}

static inline uint32_t surf_get(surface_t *surf, int x, int y) {
    if (x >= surf->width || y >= surf->height) return 0;
    return surf->pixels[y * surf->pitch + x];
}

static inline void surf_fill(surface_t *surf, uint32_t color) {
    for (int i = 0; i < surf->width * surf->height; i++) {
        surf->pixels[i] = color;
    }
}

static inline void surf_fill_rect(surface_t *surf, rect_t rect, uint32_t color) {
    if (rect.x < 0){ rect.w += rect.x; rect.x = 0; }
    if (rect.y < 0){ rect.h += rect.y; rect.y = 0; }
    if (rect.x + rect.w > surf->width)  rect.w = surf->width - rect.x;
    if (rect.y + rect.h > surf->height) rect.h = surf->height - rect.y;
    
    for (int y=0; y<rect.h; y++){
        uint32_t *row = &surf->pixels[(rect.y+y)*surf->pitch + rect.x];
        for (int x=0; x<rect.w; x++) row[x] = color;
    }
}

static inline void surf_blit(surface_t *dst, int dx, int dy,
                             surface_t *src, int sx, int sy, int w, int h) {
    // Clip source rectangle
    if (sx < 0){ w += sx; dx -= sx; sx = 0; }
    if (sy < 0){ h += sy; dy -= sy; sy = 0; }
    if (sx + w > src->width)  w = src->width  - sx;
    if (sy + h > src->height) h = src->height - sy;
    // clip to dst
    if (dx < 0){ w += dx; sx -= dx; dx = 0; }
    if (dy < 0){ h += dy; sy -= dy; dy = 0; }
    if (dx + w > dst->width)  w = dst->width  - dx;
    if (dy + h > dst->height) h = dst->height - dy;
    if (w<=0 || h<=0) return;

    for (int y=0; y<h; y++){
        uint32_t *drow = &dst->pixels[(dy+y)*dst->pitch + dx];
        uint32_t *srow = &src->pixels[(sy+y)*src->pitch + sx];
        for (int x=0; x<w; x++) drow[x] = srow[x];
    }
}

static inline bool in_titlebar(window_t *win, int x, int y) {
    return (x >= win->rect.x && x < win->rect.x + win->rect.w &&
            y >= win->rect.y && y < win->rect.y + TITLEBAR_HEIGHT);
}

static inline bool point_in_rect(int x, int y, rect_t rect) {
    return (x >= rect.x && x < rect.x + rect.w &&
            y >= rect.y && y < rect.y + rect.h);
}

static inline rect_t close_btn_rect(window_t *win) {
    return (rect_t){ win->rect.w - 18, 2, 16, 16 };
}

window_t *create_window(int x, int y, int width, int height, const char *title);
void destroy_window(window_t *window);

void default_window_event_handler(window_t *win, input_event_t *evt);

int is_pixel_set(unsigned char *glyph, int x);
void draw_char(surface_t *surf, char c, int x, int y, uint32_t fg, psf_font_t *font);
void draw_text(surface_t *surf, const char *str, int x, int y, uint32_t fg, psf_font_t *font);

void window_draw_decorations(window_t *win);
void window_draw_widgets(window_t *win);

widget_t* find_widget_at(window_t *win, int x, int y);
void window_composite(window_t *win, surface_t *target, rect_t area);