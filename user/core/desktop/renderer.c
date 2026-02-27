/*
 * renderer.c — Backbuffer helpers, window compositing, taskbar, cursor,
 *              and final blit to the hardware framebuffer.
 *
 * All bb_* functions are file-private; only composite_all() is exported.
 */
#include "user/ui.h"
#include "libc/string.h"
#include "types.h"
#include "state.h"
#include "windows.h"
#include "renderer.h"

/* -----------------------------------------------------------------------
 * Backbuffer pixel helpers (private)
 * --------------------------------------------------------------------- */

static inline void bb_put(int x, int y, uint32_t color) {
    if ((uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    backbuffer[y * fb_width + x] = color;
}

static void bb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    ui_fill_rect(backbuffer, (int)fb_width, (int)fb_width, (int)fb_height,
                 x, y, w, h, color);
}

static int bb_draw_string(int x, int y, const char *s, uint32_t fg) {
    return ui_draw_string(backbuffer, (int)fb_width,
                          (int)fb_width, (int)fb_height,
                          x, y, s, fg, 0, 1);
}

static void blit_to_fb(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if ((uint32_t)(x + w) > fb_width)  w = (int)fb_width  - x;
    if ((uint32_t)(y + h) > fb_height) h = (int)fb_height - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32_t *bb_row = backbuffer  + (y + row) * fb_width  + x;
        uint32_t *fb_row = framebuffer + (y + row) * fb_stride + x;
        for (int col = 0; col < w; col++)
            fb_row[col] = bb_row[col];
    }
}

/* -----------------------------------------------------------------------
 * Desktop background — subtle horizontal stripes
 * --------------------------------------------------------------------- */

static void draw_background(void) {
    int content_h = (int)fb_height - TASKBAR_H;
    for (int row = 0; row < content_h; row++) {
        uint32_t col = (row & 32) ? COLOR_DESKTOP_STRIPE : COLOR_DESKTOP;
        uint32_t *p = backbuffer + row * fb_width;
        for (int c = 0; c < (int)fb_width; c++)
            p[c] = col;
    }
}

/* -----------------------------------------------------------------------
 * Single-window compositing
 * --------------------------------------------------------------------- */

static void composite_window(comp_window_t *win) {
    int wx = win->x, wy = win->y;
    int ww = win->w, wh = win->h;
    int focused = (win->id == focused_win_id);

    uint32_t col_border = focused ? COLOR_BORDER_F     : COLOR_BORDER;
    uint32_t col_title  = focused ? COLOR_TITLEBAR_F   : COLOR_TITLEBAR;
    uint32_t col_txt    = focused ? COLOR_TITLE_TEXT   : COLOR_TITLE_TEXT_UF;

    /* Outer border */
    bb_fill_rect(wx - BORDER_W, wy - BORDER_W,
                 ww + 2 * BORDER_W, 1, col_border);
    bb_fill_rect(wx - BORDER_W, wy + wh + TITLE_H,
                 ww + 2 * BORDER_W, 1, col_border);
    bb_fill_rect(wx - BORDER_W, wy - BORDER_W,
                 1, wh + TITLE_H + BORDER_W, col_border);
    bb_fill_rect(wx + ww, wy - BORDER_W,
                 1, wh + TITLE_H + BORDER_W, col_border);

    /* Title bar */
    bb_fill_rect(wx, wy, ww, TITLE_H, col_title);

    /* Title text — clip to leave room for close button */
    int title_max_px = ww - 22;
    if (title_max_px > 0) {
        char clipped[65];
        strncpy(clipped, win->title, 64);
        clipped[64] = '\0';
        while (ui_string_width(clipped) > title_max_px && clipped[0])
            clipped[strlen(clipped) - 1] = '\0';
        bb_draw_string(wx + 6, wy + (TITLE_H - UI_FONT_H) / 2,
                       clipped, col_txt);
    }

    /* Close button (⊗) */
    int bx = wx + ww - 14;
    int by = wy + (TITLE_H - 12) / 2;
    bb_fill_rect(bx, by, 12, 12, COLOR_BTN_CLOSE);
    for (int d = 2; d <= 9; d++) {
        bb_put(bx + d - 2, by + d, COLOR_BTN_X);
        bb_put(bx + 11 - d, by + d, COLOR_BTN_X);
    }

    /* Client SHM pixel content */
    if (win->pixels) {
        for (int row = 0; row < wh; row++) {
            uint32_t *src = win->pixels + row * ww;
            int        dy = wy + TITLE_H + row;
            if ((uint32_t)dy >= fb_height) break;
            for (int col = 0; col < ww; col++) {
                int dx = wx + col;
                if ((uint32_t)dx < fb_width)
                    backbuffer[dy * fb_width + dx] = src[col];
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Taskbar
 * --------------------------------------------------------------------- */

static void draw_taskbar(void) {
    int ty = (int)fb_height - TASKBAR_H;

    bb_fill_rect(0, ty, (int)fb_width, TASKBAR_H, COLOR_TASKBAR);
    bb_fill_rect(0, ty, (int)fb_width, 1, COLOR_TASKBAR_SEP);

    /* Brand */
    bb_draw_string(8, ty + (TASKBAR_H - UI_FONT_H) / 2,
                   "ZineOS", COLOR_TASKBAR_LOGO);
    bb_fill_rect(TASKBAR_LOGO_W, ty + 4, 1, TASKBAR_H - 8, COLOR_TASKBAR_SEP);

    /* Window task buttons */
    int btn_x = TASKBAR_LOGO_W + 4;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].active) continue;
        if (btn_x + TASKBAR_BTN_W > (int)fb_width - TASKBAR_LAUNCH_W - 8)
            break;
        uint32_t bc = (windows[i].id == focused_win_id)
                    ? COLOR_TASKBAR_BTN_F : COLOR_TASKBAR_BTN;
        bb_fill_rect(btn_x, ty + 3, TASKBAR_BTN_W, TASKBAR_H - 6, bc);

        char label[14];
        strncpy(label, windows[i].title, 13);
        label[13] = '\0';
        while (ui_string_width(label) > TASKBAR_BTN_W - 8 && label[0])
            label[strlen(label) - 1] = '\0';
        bb_draw_string(btn_x + 4, ty + (TASKBAR_H - UI_FONT_H) / 2,
                       label, COLOR_TASKBAR_TEXT);
        btn_x += TASKBAR_BTN_W + 4;
    }

    /* App-launch button */
    int lbx = (int)fb_width - TASKBAR_LAUNCH_W - 4;
    bb_fill_rect(lbx, ty + 3, TASKBAR_LAUNCH_W, TASKBAR_H - 6,
                 COLOR_TASKBAR_BTN_H);
    bb_draw_string(lbx + 4, ty + (TASKBAR_H - UI_FONT_H) / 2,
                   "Notepad", COLOR_TASKBAR_TEXT);
}

/* -----------------------------------------------------------------------
 * Hardware-style 16×16 arrow cursor sprite
 * --------------------------------------------------------------------- */

#define CUR_H 16
#define CUR_W 16

static const char cursor_shape[CUR_H][CUR_W + 1] = {
    "X               ",
    "X.X             ",
    "X..X            ",
    "X...X           ",
    "X....X          ",
    "X.....X         ",
    "X......X        ",
    "X.......X       ",
    "X........X      ",
    "X....XXXXX      ",
    "X..X..X         ",
    "X.X  X..X       ",
    "XX    X..X      ",
    "X      X..X     ",
    "        X..X    ",
    "         XX     ",
};

static void draw_cursor(void) {
    for (int row = 0; row < CUR_H; row++)
        for (int col = 0; col < CUR_W; col++) {
            char c = cursor_shape[row][col];
            if      (c == 'X') bb_put(cursor_x + col, cursor_y + row, 0xFFFFFFFF);
            else if (c == '.') bb_put(cursor_x + col, cursor_y + row, 0xFF000000);
        }
}

/* -----------------------------------------------------------------------
 * Full-screen composite (exported)
 * --------------------------------------------------------------------- */

void composite_all(void) {
    draw_background();
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].active) composite_window(&windows[i]);
    draw_taskbar();
    draw_cursor();
    blit_to_fb(0, 0, (int)fb_width, (int)fb_height);
}
