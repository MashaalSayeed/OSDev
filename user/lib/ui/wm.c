/*
 * wm.c — User-space client library for the ZineOS window manager.
 */

#include "user/wm.h"
#include "user/syscall.h"
#include "libc/string.h"

static int wm_fd = -1;

/* -----------------------------------------------------------------------
 * Connection
 * --------------------------------------------------------------------- */

int wm_connect(void) {
    wm_fd = syscall_open("/DEV/WM", O_RDWR);
    if (wm_fd < 0) return -1;

    /* Tell the compositor a new client has arrived */
    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = WM_REQ_CONNECT;
    syscall_write(wm_fd, (const char *)&req, sizeof(req));

    /* Wait for WM_RESP_OK */
    wm_response_t resp;
    int n = syscall_read(wm_fd, (char *)&resp, sizeof(resp));
    if (n != (int)sizeof(wm_response_t) || resp.type != WM_RESP_OK)
        return -1;

    return 0;
}

void wm_disconnect(void) {
    if (wm_fd < 0) return;
    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = WM_REQ_DISCONNECT;
    syscall_write(wm_fd, (const char *)&req, sizeof(req));
    syscall_close(wm_fd);
    wm_fd = -1;
}

/* -----------------------------------------------------------------------
 * Window management
 * --------------------------------------------------------------------- */

uint32_t wm_create_window(int x, int y, int w, int h,
                           const char *title, uint32_t **pixels_out) {
    if (wm_fd < 0) return 0;

    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = WM_REQ_CREATE;
    req.x    = x;
    req.y    = y;
    req.w    = w;
    req.h    = h;
    if (title) strncpy(req.title, title, sizeof(req.title) - 1);
    syscall_write(wm_fd, (const char *)&req, sizeof(req));

    /* Read the compositor's response (contains shm_id for the pixel buffer) */
    wm_response_t resp;
    int n = syscall_read(wm_fd, (char *)&resp, sizeof(resp));
    if (n != (int)sizeof(wm_response_t) || resp.type != WM_RESP_OK)
        return 0;
    if (resp.shm_id < 0)
        return 0;

    /*
     * Map the SHM buffer into our address space.
     * After this, pixels_out[0..w*h-1] points at the same physical frames
     * as the compositor's copy – no data transfer on flush, just a notify.
     */
    if (pixels_out) {
        *pixels_out = (uint32_t *)syscall_shm_map(resp.shm_id);
        if (!*pixels_out) return 0;
    }

    return resp.win_id;
}

void wm_flush(uint32_t win_id, int x, int y, int w, int h) {
    if (wm_fd < 0) return;
    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type   = WM_REQ_FLUSH;
    req.win_id = win_id;
    req.x      = x;
    req.y      = y;
    req.w      = w;
    req.h      = h;
    syscall_write(wm_fd, (const char *)&req, sizeof(req));
}

void wm_move_window(uint32_t win_id, int x, int y) {
    if (wm_fd < 0) return;
    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type   = WM_REQ_MOVE;
    req.win_id = win_id;
    req.x      = x;
    req.y      = y;
    syscall_write(wm_fd, (const char *)&req, sizeof(req));
}

void wm_destroy_window(uint32_t win_id) {
    if (wm_fd < 0) return;
    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type   = WM_REQ_DESTROY;
    req.win_id = win_id;
    syscall_write(wm_fd, (const char *)&req, sizeof(req));
}

/* -----------------------------------------------------------------------
 * Event polling
 * --------------------------------------------------------------------- */

int wm_poll_event(wm_event_t *evt) {
    if (wm_fd < 0 || !evt) return 0;

    wm_response_t resp;
    int n = syscall_read(wm_fd, (char *)&resp, sizeof(resp));
    if (n != (int)sizeof(wm_response_t) || resp.type != WM_RESP_EVENT)
        return 0;

    evt->type   = resp.event_type;
    evt->win_id = resp.win_id;
    evt->x      = resp.event_x;
    evt->y      = resp.event_y;
    evt->key    = resp.event_key;
    evt->button = resp.event_btn;
    return 1;
}
