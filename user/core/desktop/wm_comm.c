/*
 * wm_comm.c — /DEV/WM protocol: response writes, app spawning, and the
 *             main request dispatcher that routes frames to sub-handlers.
 */
#include "user/syscall.h"
#include "common/wm_proto.h"
#include "libc/string.h"
#include "types.h"
#include "state.h"
#include "windows.h"
#include "input.h"
#include "wm_comm.h"

/* -----------------------------------------------------------------------
 * Send a response frame back to a client process
 * --------------------------------------------------------------------- */

void send_response(uint32_t target_pid, wm_response_t *resp) {
    wm_frame_t frame;
    frame.pid  = target_pid;
    frame.resp = *resp;
    syscall_write(wm_fd, (const char *)&frame, sizeof(frame));
}

/* -----------------------------------------------------------------------
 * Launch a user-space application as a forked child
 * --------------------------------------------------------------------- */

void launch_app(const char *path) {
    int pid = syscall_fork();
    if (pid == 0) {
        syscall_close(wm_fd);   /* don't leak compositor's fd to child */
        syscall_exec(path, NULL);
        syscall_exit(1);
    }
}

/* -----------------------------------------------------------------------
 * Main request dispatcher
 * --------------------------------------------------------------------- */

void handle_request(uint32_t client_pid, const wm_request_t *req) {
    wm_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.win_id = req->win_id;

    /* Input event injected by the kernel input driver (pid == 0) */
    if (client_pid == 0 && req->type == WM_REQ_INPUT) {
        handle_input(req);
        return;
    }

    switch ((wm_req_type_t)req->type) {

    /* ---- Client handshake ---- */
    case WM_REQ_CONNECT:
        resp.type   = WM_RESP_OK;
        resp.status = 0;
        send_response(client_pid, &resp);
        break;

    /* ---- Create a new window ---- */
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
            int32_t max_y = (int32_t)fb_height - TASKBAR_H
                            - TITLE_H - req->h - BORDER_W;
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

    /* ---- Mark a window's SHM dirty for the next composite pass ---- */
    case WM_REQ_FLUSH: {
        comp_window_t *win = find_window(req->win_id);
        if (win) win->dirty = 1;
        break;
    }

    /* ---- Move a window to a new screen position ---- */
    case WM_REQ_MOVE: {
        comp_window_t *win = find_window(req->win_id);
        if (win) { win->x = req->x; win->y = req->y; win->dirty = 1; }
        break;
    }

    /* ---- Destroy a window ---- */
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

    /* ---- Client disconnect ---- */
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
