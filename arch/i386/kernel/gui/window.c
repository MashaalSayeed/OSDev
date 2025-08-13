#include "kernel/window.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "kernel/framebuffer.h"

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
    win->next = NULL;
    win->prev = NULL;

    win->bitmap = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    memset(win->bitmap, 0, width * height * sizeof(uint32_t));

    return win;
}

void destroy_window(window_t *win) {
    if (!win) return;

    // Free the window structure
    kfree(win->bitmap);
    kfree(win);
}

void draw_pixel(window_t *win, int x, int y, uint32_t color) {
    if (!win || x < 0 || y < 0 || x >= win->rect.w || y >= win->rect.h) return;
    win->bitmap[y * win->rect.w + x] = color;
}

void draw_rect(window_t *win, int x, int y, int width, int height, uint32_t color) {
    if (!win) return;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            draw_pixel(win, x + j, y + i, color);
        }
    }
}

void fill_window(window_t *win, uint32_t color) {
    if (!win || !win->bitmap) return;

    for (int i = 0; i < win->rect.h; i++) {
        for (int j = 0; j < win->rect.w; j++) {
            win->bitmap[i * win->rect.w + j] = color;
        }
    }
}

int is_pixel_set(unsigned char *glyph, int x) {
    return glyph[x / 8] & (0x80 >> (x % 8));
}

void draw_char(window_t *win, char c, int x, int y, uint32_t fg, uint32_t bg, psf_font_t *font) {
    if (!win || !win->bitmap || x < 0 || y < 0 || x >= win->rect.w || y >= win->rect.h) return;

    c -= 31;
    if ((uint32_t)c >= font->numglyph) return;

    // Assuming current_font is already loaded and available
    int bytesperline = (font->width + 7) / 8;
    unsigned char *glyph = (unsigned char *)font + font->headersize + (c * font->bytesperglyph);

    for (int row = 0; row < font->height; row++) {
        for (int col = 0; col < font->width; col++) {
            if (is_pixel_set(glyph, col)) {
                draw_pixel(win, x + col, y + row, fg);
            } else {
                draw_pixel(win, x + col, y + row, bg);
            }
        }
        glyph += bytesperline;
    }
}

void draw_string(window_t *win, const char *str, int x, int y, uint32_t fg, uint32_t bg, psf_font_t *font) {
    if (!win || !win->bitmap || !str || x < 0 || y < 0) return;

    int cursor_x = x;
    int cursor_y = y;

    for (const char *s = str; *s; s++) {
        if (*s == '\n') {
            cursor_x = x;
            cursor_y += font->height;
            continue;
        }
        draw_char(win, *s, cursor_x, cursor_y, fg, bg, font);
        cursor_x += font->width;
    }
}

void draw_titlebar(window_t *win, psf_font_t *font) {
    if (!win || !win->bitmap) return;

    // Draw title bar background
    draw_rect(win, 0, 0, win->rect.w, 20, 0xCCCCCC);

    // Draw title text
    if (win->title) {
        draw_string(win, win->title, 10, 3, 0x000000, 0xCCCCCC, font);
    }
}

void draw_window_border(window_t *win, uint32_t color) {
    if (!win || !win->bitmap) return;

    draw_rect(win, 0, 0, win->rect.w, 1, color);
    draw_rect(win, 0, win->rect.h - 1, win->rect.w, 1, color);
    draw_rect(win, 0, 0, 1, win->rect.h, color);
    draw_rect(win, win->rect.w - 1, 0, 1, win->rect.h, color);
}

void draw_widgets(window_t *win) {
    if (!win || !win->widgets) return;

    for (widget_t *w = win->widgets; w; w = w->next) {
        if (w->draw) {
            // w->draw(w);
        }
    }
}

void draw_window_to_fb(window_t *win) {
    if (!win || !win->bitmap) return;

    for (int y = 0; y < win->rect.h; y++) {
        for (int x = 0; x < win->rect.w; x++) {
            uint32_t color = win->bitmap[y * win->rect.w + x];
            fb_put_pixel(win->rect.x + x, win->rect.y + y, color);
        }
    }
}