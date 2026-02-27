/*
 * windows.h — Window table lookups and hit-testing API.
 */
#ifndef DESKTOP_WINDOWS_H
#define DESKTOP_WINDOWS_H

#include <stdint.h>
#include "types.h"

/* Lookup by window ID; returns NULL if not found or inactive. */
comp_window_t *find_window(uint32_t id);

/* Return the first inactive slot in the window table, or NULL if full. */
comp_window_t *find_free_slot(void);

/* Return the topmost window index (windows[]) containing screen point
 * (sx, sy), scanning from back to front.  Returns -1 if none or if the
 * point falls inside the taskbar. */
int hit_test_idx(int sx, int sy);

/* Returns non-zero if (sx, sy) is over the close button of window w. */
int in_close_btn(comp_window_t *w, int sx, int sy);

/* Returns non-zero if (sx, sy) is inside the title bar of window w. */
int in_title_bar(comp_window_t *w, int sx, int sy);

#endif /* DESKTOP_WINDOWS_H */
