/*
 * main.c — Compositor entry point and main event/render loop.
 *
 *  1. Map the hardware framebuffer and allocate a matching backbuffer.
 *  2. Connect to /DEV/WM as the compositor process.
 *  3. Spin: drain incoming WM frames → handle requests → repaint dirty windows.
 */
#include "user/stdio.h"
#include "user/syscall.h"
#include "common/wm_proto.h"
#include "libc/string.h"
#include "types.h"
#include "state.h"
#include "renderer.h"
#include "wm_comm.h"

int main(void) {
    int a = 1+1;
    printf("[compositor] Starting\n");

    /* ---- Map hardware framebuffer ---- */
    framebuffer = (uint32_t *)syscall_fb_map(&fb_width, &fb_height, &fb_pitch);
    if (!framebuffer) {
        printf("[compositor] ERROR: fb_map failed\n");
        syscall_exit(1);
    }
    fb_stride = fb_pitch / 4;
    printf("[compositor] FB: %dx%d pitch=%d\n", fb_width, fb_height, fb_pitch);

    /* ---- Allocate backbuffer ---- */
    uint32_t bb_bytes = fb_width * fb_height * 4;
    backbuffer = (uint32_t *)syscall_sbrk((int)bb_bytes);
    if (!backbuffer) {
        printf("[compositor] ERROR: backbuffer alloc failed\n");
        syscall_exit(1);
    }
    for (uint32_t i = 0; i < fb_width * fb_height; i++)
        backbuffer[i] = COLOR_DESKTOP;

    /* ---- Connect to /DEV/WM as the compositor ---- */
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

    /* ---- Auto-launch the default application ---- */
    launch_app("/BIN/NOTEPAD");

    /* ---- Initial full-screen paint ---- */
    composite_all();

    /* ---- Main event / render loop ---- */
    static uint32_t prev_cx = 0, prev_cy = 0, prev_fid = 0;

    while (1) {
        /* Drain all pending WM frames */
        wm_frame_t frame;
        int n;
        while ((n = syscall_read(wm_fd, (char *)&frame, sizeof(frame)))
               == (int)sizeof(wm_frame_t)) {
            handle_request(frame.pid, &frame.req);
        }

        /* Repaint if any window is dirty or the cursor / focus changed */
        int dirty = 0;
        for (int i = 0; i < MAX_WINDOWS; i++)
            if (windows[i].active && windows[i].dirty) { dirty = 1; break; }

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
