#pragma once

/*
 * wm.h — User-space client library for the ZineOS window manager.
 *
 * Usage
 * -----
 *   1.  Call wm_connect() once to register with the compositor.
 *   2.  Call wm_create_window() to create a window.
 *       The compositor allocates a shared-memory pixel buffer and returns
 *       both a window ID and a pointer directly into that buffer.
 *   3.  Draw pixels into the buffer (32-bit ARGB, row-major, pitch == w).
 *   4.  Call wm_flush() to tell the compositor which region changed.
 *   5.  Poll wm_poll_event() in your event loop.
 *   6.  Call wm_destroy_window() / wm_disconnect() on cleanup.
 *
 * Example
 * -------
 *   uint32_t *pixels;
 *   wm_connect();
 *   uint32_t win = wm_create_window(100, 100, 320, 240, "My App", &pixels);
 *   pixels[0] = 0xFFFF0000;   // red top-left pixel
 *   wm_flush(win, 0, 0, 320, 240);
 *   wm_event_t evt;
 *   while (1) {
 *       if (wm_poll_event(&evt) && evt.type == WM_EVENT_CLOSE) break;
 *   }
 *   wm_destroy_window(win);
 *   wm_disconnect();
 */

#include <stdint.h>
#include "common/wm_proto.h"

/*
 * Client-side event (returned by wm_poll_event).
 * Mirrors the event fields in wm_response_t.
 */
typedef struct {
    uint32_t type;    /* wm_event_type_t                      */
    uint32_t win_id;
    int32_t  x, y;   /* mouse position or key scancode in x  */
    uint32_t key;     /* keyboard scancode                    */
    uint32_t button;  /* mouse button mask                    */
} wm_event_t;

/* -----------------------------------------------------------------------
 * Connection
 * --------------------------------------------------------------------- */

/*
 * Connect to the compositor.
 * Opens /DEV/WM and sends WM_REQ_CONNECT; blocks until the compositor
 * acknowledges.  Returns 0 on success, -1 on error.
 */
int wm_connect(void);

/*
 * Gracefully disconnect.
 * Sends WM_REQ_DISCONNECT and closes the file descriptor.
 */
void wm_disconnect(void);

/* -----------------------------------------------------------------------
 * Window management
 * --------------------------------------------------------------------- */

/*
 * Create a window at (x, y) with the given size and title.
 *
 * On success:
 *   - Returns a non-zero window ID.
 *   - *pixels_out is set to the base of the SHM pixel buffer
 *     (32-bit ARGB, width * height pixels, pitch == w pixels per row).
 *     Draw into this buffer, then call wm_flush().
 *
 * On failure: returns 0.
 */
uint32_t wm_create_window(int x, int y, int w, int h,
                           const char *title, uint32_t **pixels_out);

/*
 * Notify the compositor that a rectangle inside the window has changed.
 * The compositor re-composites from the SHM buffer on its next cycle.
 *
 * x, y, w, h are relative to the window's content area origin (0, 0).
 * Pass (0, 0, win_w, win_h) to mark the entire window dirty.
 */
void wm_flush(uint32_t win_id, int x, int y, int w, int h);

/*
 * Move a window to an absolute screen position.
 */
void wm_move_window(uint32_t win_id, int x, int y);

/*
 * Close and destroy a window, releasing its SHM buffer.
 */
void wm_destroy_window(uint32_t win_id);

/* -----------------------------------------------------------------------
 * Event polling
 * --------------------------------------------------------------------- */

/*
 * Non-blocking: pop the next event destined for this process.
 * Returns 1 and fills *evt if an event is available, 0 otherwise.
 */
int wm_poll_event(wm_event_t *evt);
