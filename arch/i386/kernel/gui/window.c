#include "kernel/window.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "kernel/framebuffer.h"

extern psf_font_t *current_font;

window_t * create_window(int x, int y, int width, int height, const char *title) {
    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) {
        printf("Error: Failed to allocate memory for window\n");
        return NULL;
    }

    win->rect = (rect_t){ x, y, width, height };
    win->title = title;
    win->focused = false;
    win->dirty = true;
    win->zindex = 0;
    win->event_handler = default_window_event_handler;

    win->surface.width = width;
    win->surface.height = height;
    win->surface.pitch = width;
    win->surface.pixels = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    memset(win->surface.pixels, 0, width * height * sizeof(uint32_t));

    win->content = (rect_t){ BORDER_WIDTH, TITLEBAR_HEIGHT, width - 2 * BORDER_WIDTH, height - TITLEBAR_HEIGHT - BORDER_WIDTH };
    return win;
}

void default_window_event_handler(window_t *win, input_event_t *evt) {
    switch (evt->type) {
        case INPUT_TYPE_MOUSE:
            mouse_event_t *mouse_evt = &evt->mouse;
            int local_x = mouse_evt->x - win->rect.x;
            int local_y = mouse_evt->y - win->rect.y;

            if (local_x < 0 || local_x >= win->rect.w || local_y < 0 || local_y >= win->rect.h) return; // Mouse event outside

            widget_t *hit = find_widget_at(win, local_x, local_y);
            if (hit) {
                if (mouse_evt->type == MOUSE_EVENT_DOWN && hit->on_click) {
                    hit->on_click(hit, local_x, local_y);
                }
            } else {
                // No widget hit, handle window event
                if (mouse_evt->type == MOUSE_EVENT_DOWN) {
                    printf("Window clicked at (%d, %d)\n", local_x, local_y);
                }
            }
    }
}

void destroy_window(window_t *win) {
    if (!win) return;

    // Free the window structure
    kfree(win->surface.pixels);
    kfree(win);
}

int is_pixel_set(unsigned char *glyph, int x) {
    return glyph[x / 8] & (0x80 >> (x % 8));
}

void draw_char(surface_t *surf, char c, int x, int y, uint32_t fg, psf_font_t *font) {
    if (!surf || !surf->pixels || x < 0 || y < 0 || x >= surf->width || y >= surf->height) return;

    c -= 31;
    if ((uint32_t)c >= font->numglyph) return;

    // Assuming current_font is already loaded and available
    int bytesperline = (font->width + 7) / 8;
    unsigned char *glyph = (unsigned char *)font + font->headersize + (c * font->bytesperglyph);

    for (int row = 0; row < font->height; row++) {
        for (int col = 0; col < font->width; col++) {
            if (is_pixel_set(glyph, col)) {
                surf_put(surf, x + col, y + row, fg);
            }
        }
        glyph += bytesperline;
    }
}

void draw_text(surface_t *surf, const char *str, int x, int y, uint32_t fg, psf_font_t *font) {
    if (!surf || !surf->pixels || !str || x < 0 || y < 0) return;

    int cursor_x = x;
    int cursor_y = y;
    for (const char *s = str; *s; s++) {
        if (*s == '\n') {
            cursor_x = x;
            cursor_y += font->height;
            continue;
        }
        draw_char(surf, *s, cursor_x, cursor_y, fg, font);
        cursor_x += font->width;
    }
}

void window_draw_decorations(window_t *win) {
    if (!win || !win->surface.pixels) return;

    surface_t *s = &win->surface;
    surf_fill(s, 0xFFFFFF);
    // Draw title bar
    surf_fill_rect(s, (rect_t){ 0, 0, win->rect.w, TITLEBAR_HEIGHT }, 0xCCCCCC);
    // Draw window border
    surf_fill_rect(s, (rect_t){ 0, 0, win->rect.w, BORDER_WIDTH }, 0x000000);
    surf_fill_rect(s, (rect_t){ 0, win->rect.h - BORDER_WIDTH, win->rect.w, BORDER_WIDTH }, 0x000000);
    surf_fill_rect(s, (rect_t){ 0, 0, BORDER_WIDTH, win->rect.h }, 0x000000);
    surf_fill_rect(s, (rect_t){ win->rect.w - BORDER_WIDTH, 0, BORDER_WIDTH, win->rect.h }, 0x000000);
    // Draw title text
    draw_text(s, win->title, 8, 4, 0x000000, current_font);
    // Draw close button
    rect_t close_rect = close_btn_rect(win);
    surf_fill_rect(s, close_rect, 0xFF0000);
    draw_text(s, "X", close_rect.x + 4, close_rect.y + 2, 0xFFFFFF, current_font);
}

widget_t* find_widget_at(window_t *win, int x, int y) {
    if (!win || !win->widgets) return NULL;

    for (widget_t *w = win->widgets; w; w = w->next) {
        if (w->visible && x >= w->rect.x && x < w->rect.x + w->rect.w &&
            y >= w->rect.y && y < w->rect.y + w->rect.h) {
            return w;
        }
    }
    return NULL;
}

void window_draw_widgets(window_t *win) {
    if (!win || !win->widgets) return;
    
    for (widget_t *w = win->widgets; w; w = w->next) {
        if (w->draw) {
            w->draw(w, &win->surface);
        }
    }
}

void window_composite(window_t *win, surface_t *target, rect_t area) {
    rect_t inter = rect_intersection(area, win->rect);
    if (inter.w <= 0 || inter.h <= 0) return;

    // Blit the window surface to the target surface
    surf_blit(target, inter.x, inter.y, &win->surface, inter.x - win->rect.x, inter.y - win->rect.y, inter.w, inter.h);
}

void label_draw(widget_t *self, surface_t *surf) {
    draw_text(surf, self->text, self->rect.x, self->rect.y, 0x000000, current_font);
}

widget_t *create_widget(rect_t rect) {
    widget_t *widget = (widget_t *)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;

    widget->rect = rect;
    widget->visible = true;
    return widget;
}

widget_t *create_label(int x, int y, const char *text) {
    widget_t *label = create_widget((rect_t){ x, y, 0, 0 });
    if (!label) return NULL;
    label->text = text;
    label->draw = label_draw;
    printf("Drawing label at %s\n", text);
    return label;
}