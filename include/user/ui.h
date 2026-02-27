#pragma once

/*
 * ui.h — Lightweight user-space UI drawing library for ZineOS.
 *
 * All drawing functions write into a flat 32-bit ARGB pixel buffer.
 *
 *   pixels   – pointer to the top-left pixel of the buffer
 *   pitch    – number of pixels (not bytes) per row
 *   cw, ch   – clipping width / height; pixels outside [0,cw) × [0,ch) are skipped
 *
 * Built-in font: 8 × 8 bitmap, covers ASCII 32–126.
 */

#include <stdint.h>

#define UI_FONT_W  8
#define UI_FONT_H  8

/* ── Colour helpers ─────────────────────────────────────────────────── */
#define UI_ARGB(a,r,g,b) (((uint32_t)(a)<<24)|((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b))
#define UI_RGB(r,g,b)    UI_ARGB(0xFF,r,g,b)

/* ── Drawing primitives ─────────────────────────────────────────────── */

/* Fill an axis-aligned rectangle with a solid colour. */
void ui_fill_rect(uint32_t *pixels, int pitch, int cw, int ch,
                  int x, int y, int w, int h, uint32_t color);

/*
 * Draw a single character at (x, y).
 * If transparent_bg != 0 the background colour is not written (blended
 * against whatever is already in the buffer).
 */
void ui_draw_char(uint32_t *pixels, int pitch, int cw, int ch,
                  int x, int y, char c, uint32_t fg, uint32_t bg,
                  int transparent_bg);

/*
 * Draw a NUL-terminated string starting at (x, y).
 * Returns the x coordinate just after the last glyph rendered.
 */
int ui_draw_string(uint32_t *pixels, int pitch, int cw, int ch,
                   int x, int y, const char *s,
                   uint32_t fg, uint32_t bg, int transparent_bg);

/* Pixel width of a string rendered with the built-in font. */
int ui_string_width(const char *s);
