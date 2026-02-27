/*
 * windows.c — Window table lookups and geometry hit-testing.
 */
#include "types.h"
#include "state.h"
#include "windows.h"
#include <stddef.h>

comp_window_t *find_window(uint32_t id) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].active && windows[i].id == id)
            return &windows[i];
    return NULL;
}

comp_window_t *find_free_slot(void) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i].active) return &windows[i];
    return NULL;
}

int hit_test_idx(int sx, int sy) {
    if ((uint32_t)sy >= fb_height - TASKBAR_H) return -1;
    /* Scan back-to-front so topmost window wins */
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        comp_window_t *w = &windows[i];
        if (!w->active) continue;
        int wx = w->x - BORDER_W;
        int wy = w->y - BORDER_W;
        int ww = w->w + 2 * BORDER_W;
        int wh = w->h + TITLE_H + BORDER_W;
        if (sx >= wx && sx < wx + ww && sy >= wy && sy < wy + wh)
            return i;
    }
    return -1;
}

int in_close_btn(comp_window_t *w, int sx, int sy) {
    int bx = w->x + w->w - 14;
    int by = w->y + (TITLE_H - 12) / 2;
    return sx >= bx && sx < bx + 12 && sy >= by && sy < by + 12;
}

int in_title_bar(comp_window_t *w, int sx, int sy) {
    return sx >= w->x && sx < w->x + w->w
        && sy >= w->y && sy < w->y + TITLE_H;
}
