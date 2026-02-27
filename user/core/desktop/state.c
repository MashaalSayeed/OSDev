/*
 * state.c — Definitions (storage) for all shared compositor globals.
 */
#include "types.h"
#include "state.h"
#include <stddef.h>

comp_window_t windows[MAX_WINDOWS];
uint32_t      next_win_id = 1;

int wm_fd    = -1;
int mouse_fd = -1;

uint32_t *framebuffer = NULL;
uint32_t *backbuffer  = NULL;
uint32_t  fb_width  = 0;
uint32_t  fb_height = 0;
uint32_t  fb_pitch  = 0;
uint32_t  fb_stride = 0;

int32_t cursor_x = 0;
int32_t cursor_y = 0;

uint32_t focused_win_id = 0;
int      drag_win_idx   = -1;
int32_t  drag_ox = 0;
int32_t  drag_oy = 0;
uint32_t prev_btn = 0;
