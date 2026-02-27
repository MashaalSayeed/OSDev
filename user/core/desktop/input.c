/*
 * input.c — Mouse and keyboard event processing.
 *
 * Translates raw WM_REQ_INPUT frames (cursor position, button state,
 * key code) into compositor state changes (drag, focus, close, forward).
 */
#include "user/syscall.h"
#include "common/wm_proto.h"
#include "libc/string.h"
#include "types.h"
#include "state.h"
#include "windows.h"
#include "wm_comm.h"
#include "input.h"

/* -----------------------------------------------------------------------
 * Taskbar hit-testing helpers (private)
 * --------------------------------------------------------------------- */

static int taskbar_btn_hit(int sx, int sy) {
    int ty = (int)fb_height - TASKBAR_H;
    if (sy < ty + 3 || sy > ty + TASKBAR_H - 3) return -1;
    int btn_x = TASKBAR_LOGO_W + 4;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].active) continue;
        if (btn_x + TASKBAR_BTN_W > (int)fb_width - TASKBAR_LAUNCH_W - 8)
            break;
        if (sx >= btn_x && sx < btn_x + TASKBAR_BTN_W) return i;
        btn_x += TASKBAR_BTN_W + 4;
    }
    return -1;
}

static int taskbar_launch_hit(int sx, int sy) {
    int ty  = (int)fb_height - TASKBAR_H;
    int lbx = (int)fb_width - TASKBAR_LAUNCH_W - 4;
    return sx >= lbx && sx < lbx + TASKBAR_LAUNCH_W
        && sy >= ty + 3 && sy < ty + TASKBAR_H - 3;
}

/* -----------------------------------------------------------------------
 * Input dispatch (exported)
 * --------------------------------------------------------------------- */

void handle_input(const wm_request_t *req) {
    uint32_t etype = req->win_id >> 16;
    uint32_t btn   = req->win_id & 0xFFFF;

    if (etype == WM_EVENT_MOUSE) {
        cursor_x = req->x;
        cursor_y = req->y;

        int lmb_down = ( btn & 1) && !(prev_btn & 1);  /* just pressed  */
        int lmb_up   = !(btn & 1) &&  (prev_btn & 1);  /* just released */
        int lmb_held = ( btn & 1);

        /* End drag on button release */
        if (lmb_up && drag_win_idx >= 0)
            drag_win_idx = -1;

        /* Continue drag on mouse move */
        if (lmb_held && drag_win_idx >= 0) {
            comp_window_t *dw = &windows[drag_win_idx];
            dw->x = cursor_x - drag_ox;
            dw->y = cursor_y - drag_oy;
            if (dw->x < 0) dw->x = 0;
            if (dw->y < 0) dw->y = 0;
            int max_y = (int)fb_height - TASKBAR_H - TITLE_H;
            if (dw->y > max_y) dw->y = max_y;
            dw->dirty = 1;
        }

        /* Left button just pressed */
        if (lmb_down) {
            int in_tb = ((uint32_t)cursor_y >= fb_height - TASKBAR_H);
            if (in_tb) {
                int wi = taskbar_btn_hit(cursor_x, cursor_y);
                if (wi >= 0)
                    focused_win_id = windows[wi].id;
                else if (taskbar_launch_hit(cursor_x, cursor_y))
                    launch_app("/BIN/NOTEPAD");
            } else {
                int idx = hit_test_idx(cursor_x, cursor_y);
                if (idx >= 0) {
                    comp_window_t *w = &windows[idx];
                    focused_win_id = w->id;

                    if (in_close_btn(w, cursor_x, cursor_y)) {
                        /* Send close event then destroy compositor-side state */
                        wm_response_t ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type       = WM_RESP_EVENT;
                        ev.win_id     = w->id;
                        ev.event_type = WM_EVENT_CLOSE;
                        send_response(w->owner_pid, &ev);
                        syscall_shm_unmap(w->shm_id);
                        syscall_shm_destroy(w->shm_id);
                        if (focused_win_id == w->id) focused_win_id = 0;
                        w->active = 0;
                    } else if (in_title_bar(w, cursor_x, cursor_y)) {
                        /* Begin title-bar drag */
                        drag_win_idx = idx;
                        drag_ox = cursor_x - w->x;
                        drag_oy = cursor_y - w->y;
                    } else {
                        /* Forward click into client-area */
                        wm_response_t ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type       = WM_RESP_EVENT;
                        ev.win_id     = w->id;
                        ev.event_type = WM_EVENT_MOUSE;
                        ev.event_x    = cursor_x - w->x;
                        ev.event_y    = cursor_y - (w->y + TITLE_H);
                        ev.event_btn  = btn;
                        send_response(w->owner_pid, &ev);
                    }
                }
            }
        }
        prev_btn = btn;

    } else if (etype == WM_EVENT_KEY) {
        /* Forward keyboard events to the focused window */
        if (focused_win_id) {
            comp_window_t *fw = find_window(focused_win_id);
            if (fw) {
                wm_response_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.type       = WM_RESP_EVENT;
                ev.win_id     = fw->id;
                ev.event_type = WM_EVENT_KEY;
                ev.event_key  = (uint32_t)req->w;
                send_response(fw->owner_pid, &ev);
            }
        }
    }
}
