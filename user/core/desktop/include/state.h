/*
 * state.h — Extern declarations for all shared compositor globals.
 *            Include this in every module that reads or writes shared state.
 */
#ifndef DESKTOP_STATE_H
#define DESKTOP_STATE_H

#include <stdint.h>
#include "types.h"

/* Window table */
extern comp_window_t windows[MAX_WINDOWS];
extern uint32_t      next_win_id;

/* Device file descriptors */
extern int wm_fd;
extern int mouse_fd;

/* Framebuffer / backbuffer */
extern uint32_t *framebuffer;
extern uint32_t *backbuffer;
extern uint32_t  fb_width, fb_height, fb_pitch, fb_stride;

/* Cursor position */
extern int32_t cursor_x, cursor_y;

/* Focus and drag */
extern uint32_t focused_win_id;
extern int      drag_win_idx;   /* windows[] index being dragged, -1 = none */
extern int32_t  drag_ox, drag_oy;
extern uint32_t prev_btn;

#endif /* DESKTOP_STATE_H */
