/*
 * compositor.c — User-space Window Server / Compositor for ZineOS
 *
 * Architecture
 * ============
 *
 *  ┌──────────────┐    SHM pixel buffer (shared physical pages)
 *  │  Client app  │ ──────────────────────────────────────────────┐
 *  └──────┬───────┘                                               │
 *         │ write(wm_fd, WM_REQ_CREATE / WM_REQ_FLUSH)           │
 *         ▼                                                       ▼
 *  ┌──────────────┐                                    ┌──────────────────┐
 *  │  /DEV/WM     │  ◄──── wm_frame_t inbox ────────── │   Compositor     │
 *  │  (kernel     │  ───── per-client resp ring ──────► │   (this file)    │
 *  │   mailbox)   │                                    └────────┬─────────┘
 *  └──────────────┘                                             │
 *                                               composite SHMs  │ blit
 *                                               onto backbuffer  ▼
 *                                                    ┌──────────────────┐
 *                                                    │  Physical FB     │
 *                                                    └──────────────────┘
 *
 * Features
 * --------
 *  • Tiled desktop background (dark blue-grey)
 *  • Desktop taskbar at the bottom (window list + Notepad launcher)
 *  • Window chrome: border + title bar (focused/unfocused colours)
 *    – Window title rendered with the embedded 8×8 font
 *    – Red close button (⊗) top-right of title bar
 *  • Window dragging from the title bar
 *  • Focus tracking: clicking a window or its taskbar button focuses it
 *  • Keyboard events forwarded to the focused window
 *  • Hardware cursor (16×16 arrow sprite)
 *  • Auto-launches /BIN/NOTEPAD on start
 *  • Taskbar "Notepad" button spawns additional instances
 */

#include "user/stdio.h"
#include "user/syscall.h"
#include "user/stdlib.h"
#include "libc/string.h"
#include "common/syscall.h"
#include "common/wm_proto.h"
#include "user/ui.h"

/* -----------------------------------------------------------------------
 * Build-time constants
 * --------------------------------------------------------------------- */
#define MAX_WINDOWS      32
#define TITLE_H          22      /* title bar height in pixels             */
#define BORDER_W          1      /* window border width in pixels          */
#define TASKBAR_H        28      /* taskbar height in pixels               */
#define TASKBAR_BTN_W   110      /* width of each taskbar window button    */
#define TASKBAR_LOGO_W   64      /* width of the logo / brand area         */
#define TASKBAR_LAUNCH_W 80      /* width of the "Notepad" launch button   */

/* -----------------------------------------------------------------------
 * Colour palette
 * --------------------------------------------------------------------- */
#define COLOR_DESKTOP        0xFF1C3B5A
#define COLOR_DESKTOP_STRIPE 0xFF193451

#define COLOR_BORDER         0xFF5A8AB0
#define COLOR_BORDER_F       0xFF89B8D8

#define COLOR_TITLEBAR       0xFF2E5C80
#define COLOR_TITLEBAR_F     0xFF1A7FBF
#define COLOR_TITLE_TEXT     0xFFDDEEFF
#define COLOR_TITLE_TEXT_UF  0xFF8AACCC

#define COLOR_BTN_CLOSE      0xFFE05050
#define COLOR_BTN_X          0xFFFFFFFF

#define COLOR_TASKBAR        0xFF111E2A
#define COLOR_TASKBAR_SEP    0xFF2A4A66
#define COLOR_TASKBAR_BTN    0xFF1E3A52
#define COLOR_TASKBAR_BTN_F  0xFF1A7FBF
#define COLOR_TASKBAR_BTN_H  0xFF2A5070
#define COLOR_TASKBAR_TEXT   0xFFCCDDEE
#define COLOR_TASKBAR_LOGO   0xFF89B8D8

#define COLOR_WIN_BG         0xFFFFFFFF

/* -----------------------------------------------------------------------
 * Per-window state (compositor-private)
 * --------------------------------------------------------------------- */
typedef struct {
    uint32_t  id;
    int32_t   x, y;       /* screen position of the title-bar top-left   */
    int32_t   w, h;       /* client content area size                     */
    int32_t   shm_id;
    uint32_t *pixels;     /* compositor VA into the SHM pixel buffer      */
    uint32_t  owner_pid;
    int       active;
    int       dirty;
    char      title[64];
} comp_window_t;

static comp_window_t windows[MAX_WINDOWS];
static uint32_t      next_win_id = 1;
static int           wm_fd = -1;
static int           mouse_fd = -1;   

/* Framebuffer / backbuffer */
static uint32_t *framebuffer = NULL;
static uint32_t *backbuffer  = NULL;
static uint32_t  fb_width, fb_height, fb_pitch;
static uint32_t  fb_stride;

/* Mouse cursor position */
static int32_t   cursor_x = 0, cursor_y = 0;

/* Focus & drag state */
static uint32_t  focused_win_id = 0;
static int       drag_win_idx   = -1;  /* windows[] index being dragged */
static int32_t   drag_ox = 0, drag_oy = 0;
static uint32_t  prev_btn = 0;

/* -----------------------------------------------------------------------
 * Backbuffer pixel helpers
 * --------------------------------------------------------------------- */
static inline void bb_put(int x, int y, uint32_t color) {
    if ((uint32_t)x >= fb_width || (uint32_t)y >= fb_height) return;
    backbuffer[y * fb_width + x] = color;
}

static void bb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    ui_fill_rect(backbuffer, (int)fb_width, (int)fb_width, (int)fb_height,
                 x, y, w, h, color);
}

/* Draw a string into the backbuffer at screen coordinates (transparent bg). */
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
        uint32_t *bb_row = backbuffer  + (y + row) * fb_width   + x;
        uint32_t *fb_row = framebuffer + (y + row) * fb_stride  + x;
        for (int col = 0; col < w; col++)
            fb_row[col] = bb_row[col];
    }
}

/* -----------------------------------------------------------------------
 * Window helpers
 * --------------------------------------------------------------------- */
static comp_window_t *find_window(uint32_t id) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].active && windows[i].id == id)
            return &windows[i];
    return NULL;
}

static comp_window_t *find_free_slot(void) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i].active) return &windows[i];
    return NULL;
}

/* Return window index (topmost first) containing screen point (sx,sy).
 * Returns -1 if no window is hit or the point is in the taskbar. */
static int hit_test_idx(int sx, int sy) {
    if ((uint32_t)sy >= fb_height - TASKBAR_H) return -1;
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

static int in_close_btn(comp_window_t *w, int sx, int sy) {
    int bx = w->x + w->w - 14;
    int by = w->y + (TITLE_H - 12) / 2;
    return sx >= bx && sx < bx + 12 && sy >= by && sy < by + 12;
}

static int in_title_bar(comp_window_t *w, int sx, int sy) {
    return sx >= w->x && sx < w->x + w->w &&
           sy >= w->y && sy < w->y + TITLE_H;
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
 * Compositing
 * --------------------------------------------------------------------- */
static void composite_window(comp_window_t *win) {
    int wx = win->x, wy = win->y;
    int ww = win->w, wh = win->h;
    int focused = (win->id == focused_win_id);

    uint32_t col_border = focused ? COLOR_BORDER_F       : COLOR_BORDER;
    uint32_t col_title  = focused ? COLOR_TITLEBAR_F     : COLOR_TITLEBAR;
    uint32_t col_txt    = focused ? COLOR_TITLE_TEXT     : COLOR_TITLE_TEXT_UF;

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

    /* Window title text (clipped to leave room for close button) */
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

    /* Close button */
    int bx = wx + ww - 14;
    int by = wy + (TITLE_H - 12) / 2;
    bb_fill_rect(bx, by, 12, 12, COLOR_BTN_CLOSE);
    for (int d = 2; d <= 9; d++) {
        bb_put(bx + d - 2, by + d,  COLOR_BTN_X);
        bb_put(bx + 11-d,  by + d,  COLOR_BTN_X);
    }

    /* Client SHM content */
    if (win->pixels) {
        for (int row = 0; row < wh; row++) {
            uint32_t *src = win->pixels + row * ww;
            int        by2 = wy + TITLE_H + row;
            if ((uint32_t)by2 >= fb_height) break;
            for (int col = 0; col < ww; col++) {
                int bx2 = wx + col;
                if ((uint32_t)bx2 < fb_width)
                    backbuffer[by2 * fb_width + bx2] = src[col];
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
    bb_fill_rect(0, ty, (int)fb_width, 1, COLOR_TASKBAR_SEP);   /* top edge */

    /* Brand label */
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

    /* Notepad launch button */
    int lbx = (int)fb_width - TASKBAR_LAUNCH_W - 4;
    bb_fill_rect(lbx, ty + 3, TASKBAR_LAUNCH_W, TASKBAR_H - 6,
                 COLOR_TASKBAR_BTN_H);
    bb_draw_string(lbx + 4, ty + (TASKBAR_H - UI_FONT_H) / 2,
                   "Notepad", COLOR_TASKBAR_TEXT);
}

/* -----------------------------------------------------------------------
 * Cursor sprite (16×16 hardware-style arrow)
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

static void composite_all(void) {
    draw_background();
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].active) composite_window(&windows[i]);
    draw_taskbar();
    draw_cursor();
    blit_to_fb(0, 0, (int)fb_width, (int)fb_height);
}

/* -----------------------------------------------------------------------
 * Send a response frame to a client process
 * --------------------------------------------------------------------- */
static void send_response(uint32_t target_pid, wm_response_t *resp) {
    wm_frame_t frame;
    frame.pid  = target_pid;
    frame.resp = *resp;
    syscall_write(wm_fd, (const char *)&frame, sizeof(frame));
}

/* -----------------------------------------------------------------------
 * Launch a user-space app as a forked child
 * --------------------------------------------------------------------- */
static void launch_app(const char *path) {
    int pid = syscall_fork();
    if (pid == 0) {
        syscall_close(wm_fd);   /* don't leak compositor's fd to child */
        syscall_exec(path, NULL);
        syscall_exit(1);
    }
}

/* -----------------------------------------------------------------------
 * Taskbar hit-testing helpers
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
    return sx >= lbx && sx < lbx + TASKBAR_LAUNCH_W &&
           sy >= ty + 3 && sy < ty + TASKBAR_H - 3;
}

/* -----------------------------------------------------------------------
 * Input event handling
 * --------------------------------------------------------------------- */
static void handle_input(const wm_request_t *req) {
    uint32_t etype = req->win_id >> 16;
    uint32_t btn   = req->win_id & 0xFFFF;

    if (etype == WM_EVENT_MOUSE) {
        cursor_x = req->x;
        cursor_y = req->y;

        int lmb_down = ( btn & 1) && !(prev_btn & 1);   /* just pressed  */
        int lmb_up   = !(btn & 1) &&  (prev_btn & 1);   /* just released */
        int lmb_held = ( btn & 1);

        /* End drag on release */
        if (lmb_up && drag_win_idx >= 0)
            drag_win_idx = -1;

        /* Continue drag on move */
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
                        /* Forward click to client */
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

static void handle_request(uint32_t client_pid, const wm_request_t *req) {
    wm_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.win_id = req->win_id;

    if (client_pid == 0 && req->type == WM_REQ_INPUT) {
        handle_input(req);
        return;
    }

    switch ((wm_req_type_t)req->type) {

    case WM_REQ_CONNECT:
        resp.type   = WM_RESP_OK;
        resp.status = 0;
        send_response(client_pid, &resp);
        break;

    case WM_REQ_CREATE: {
        comp_window_t *win = find_free_slot();
        if (!win) {
            resp.type = WM_RESP_ERROR; resp.status = -1;
            send_response(client_pid, &resp);
            break;
        }

        int32_t shm_id = syscall_shm_create((uint32_t)(req->w * req->h * 4));
        if (shm_id < 0) {
            resp.type = WM_RESP_ERROR; resp.status = -1;
            send_response(client_pid, &resp);
            break;
        }

        uint32_t *comp_pixels = (uint32_t *)syscall_shm_map(shm_id);
        if (!comp_pixels) {
            resp.type = WM_RESP_ERROR; resp.status = -1;
            send_response(client_pid, &resp);
            break;
        }

        for (int i = 0; i < req->w * req->h; i++)
            comp_pixels[i] = COLOR_WIN_BG;

        /* Clamp initial position inside the visible desktop area */
        int32_t cx = req->x, cy = req->y;
        if (cx < BORDER_W) cx = BORDER_W;
        if (cy < BORDER_W) cy = BORDER_W;
        {
            int32_t max_y = (int32_t)fb_height - TASKBAR_H - TITLE_H - req->h - BORDER_W;
            if (cy > max_y && max_y > 0) cy = max_y;
        }

        win->id        = next_win_id++;
        win->x         = cx;
        win->y         = cy;
        win->w         = req->w;
        win->h         = req->h;
        win->shm_id    = shm_id;
        win->pixels    = comp_pixels;
        win->owner_pid = client_pid;
        win->active    = 1;
        win->dirty     = 1;
        strncpy(win->title, req->title, sizeof(win->title) - 1);
        focused_win_id = win->id;   /* new windows steal focus */

        resp.type   = WM_RESP_OK;
        resp.win_id = win->id;
        resp.shm_id = shm_id;
        resp.status = 0;
        send_response(client_pid, &resp);
        break;
    }

    case WM_REQ_FLUSH: {
        comp_window_t *win = find_window(req->win_id);
        if (win) win->dirty = 1;
        break;
    }

    case WM_REQ_MOVE: {
        comp_window_t *win = find_window(req->win_id);
        if (win) { win->x = req->x; win->y = req->y; win->dirty = 1; }
        break;
    }

    case WM_REQ_DESTROY: {
        comp_window_t *win = find_window(req->win_id);
        if (win) {
            syscall_shm_unmap(win->shm_id);
            syscall_shm_destroy(win->shm_id);
            if (focused_win_id == win->id) focused_win_id = 0;
            win->active = 0;
        }
        resp.type = WM_RESP_OK; resp.status = 0;
        send_response(client_pid, &resp);
        break;
    }

    case WM_REQ_DISCONNECT:
        resp.type = WM_RESP_OK;
        send_response(client_pid, &resp);
        break;

    default:
        resp.type = WM_RESP_ERROR; resp.status = -1;
        send_response(client_pid, &resp);
        break;
    }
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */
int main(void) {
    printf("[compositor] Starting\n");

    framebuffer = (uint32_t *)syscall_fb_map(&fb_width, &fb_height, &fb_pitch);
    if (!framebuffer) {
        printf("[compositor] ERROR: fb_map failed\n");
        syscall_exit(1);
    }
    fb_stride = fb_pitch / 4;
    printf("[compositor] FB: %dx%d pitch=%d\n", fb_width, fb_height, fb_pitch);

    uint32_t bb_bytes = fb_width * fb_height * 4;
    backbuffer = (uint32_t *)syscall_sbrk((int)bb_bytes);
    if (!backbuffer) {
        printf("[compositor] ERROR: backbuffer alloc failed\n");
        syscall_exit(1);
    }
    for (uint32_t i = 0; i < fb_width * fb_height; i++)
        backbuffer[i] = COLOR_DESKTOP;

    wm_fd = syscall_open("/DEV/WM", O_RDWR);
    if (wm_fd < 0) {
        printf("[compositor] ERROR: cannot open /DEV/WM\n");
        syscall_exit(1);
    }
    wm_request_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.type = WM_REQ_CONNECT;
    syscall_write(wm_fd, (const char *)&conn, sizeof(conn));
    printf("[compositor] Registered (pid=%d)\n", syscall_getpid());

    /* Initial desktop paint */
    composite_all();

    /* ---- Main event / render loop ---- */
    while (1) {
        wm_frame_t frame;
        int n;
        while ((n = syscall_read(wm_fd, (char *)&frame, sizeof(frame)))
               == (int)sizeof(wm_frame_t)) {
            handle_request(frame.pid, &frame.req);
        }

        /* Repaint whenever any window is dirty */
        int dirty = 0;
        for (int i = 0; i < MAX_WINDOWS; i++)
            if (windows[i].active && windows[i].dirty) { dirty = 1; break; }

        /* Always repaint when cursor position or focus changes */
        static uint32_t prev_cx = 0, prev_cy = 0, prev_fid = 0;
        if ((uint32_t)cursor_x != prev_cx || (uint32_t)cursor_y != prev_cy ||
                focused_win_id != prev_fid)
            dirty = 1;

        if (dirty) {
            composite_all();
            for (int i = 0; i < MAX_WINDOWS; i++) windows[i].dirty = 0;
            prev_cx  = (uint32_t)cursor_x;
            prev_cy  = (uint32_t)cursor_y;
            prev_fid = focused_win_id;
        }

        syscall_yield();
    }

    return 0;
}
