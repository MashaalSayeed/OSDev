/*
 * guitest.c — WM + framebuffer test app for ZineOS.
 *
 * Default: create a WM window and render an animated test scene.
 * Raw mode: map the framebuffer directly and render a full-screen test.
 */

#include "user/stdio.h"
#include "user/syscall.h"
#include "user/wm.h"
#include "user/ui.h"
#include "common/wm_proto.h"
#include "libc/string.h"
#include <stddef.h>

#define WIN_W 420
#define WIN_H 260
#define WIN_X 80
#define WIN_Y 60

#define COL_BG      0xFF101820
#define COL_PANEL   0xFF182432
#define COL_TEXT    0xFFE6EEF6
#define COL_ACCENT  0xFF4CC3FF
#define COL_RED     0xFFDD4A4A
#define COL_GREEN   0xFF49C97A
#define COL_BLUE    0xFF3E6CFF
#define COL_YELLOW  0xFFF2C94C

static uint32_t win_id = 0;
static uint32_t *pixels = NULL;
static int last_mouse_x = -1;
static int last_mouse_y = -1;

static int box_x = 24;
static int box_y = 24;
static int box_dx = 2;
static int box_dy = 2;

static void u32_to_str(uint32_t v, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    char tmp[12];
    int t = 0;
    if (v == 0) {
        tmp[t++] = '0';
    } else {
        while (v > 0 && t < (int)sizeof(tmp)) {
            tmp[t++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    size_t n = 0;
    while (t > 0 && n + 1 < out_sz) {
        out[n++] = tmp[--t];
    }
    out[n] = '\0';
}

static void draw_checker(uint32_t *buf, int pitch, int w, int h) {
    int cell = 12;
    for (int y = 0; y < h; y += cell) {
        for (int x = 0; x < w; x += cell) {
            uint32_t c = ((x / cell) ^ (y / cell)) & 1 ? 0xFF0F151D : 0xFF151C26;
            ui_fill_rect(buf, pitch, w, h, x, y, cell, cell, c);
        }
    }
}

static void draw_gradient(uint32_t *buf, int pitch, int w, int h) {
    for (int y = 0; y < h; y++) {
        uint8_t g = (uint8_t)(40 + (y * 140) / (h > 1 ? (h - 1) : 1));
        uint32_t c = UI_RGB(g, (uint8_t)(g + 20), (uint8_t)(g + 40));
        uint32_t *row = buf + y * pitch;
        for (int x = 0; x < w; x++) row[x] = c;
    }
}

static void render_scene(uint32_t *buf, int pitch, int w, int h) {
    draw_checker(buf, pitch, w, h);

    ui_fill_rect(buf, pitch, w, h, 12, 12, w - 24, h - 24, COL_PANEL);
    ui_fill_rect(buf, pitch, w, h, 18, 18, w - 36, 32, 0xFF0F1E2B);

    ui_draw_string(buf, pitch, w, h, 24, 24,
                   "GUI Test: WM + SHM", COL_TEXT, 0xFF0F1E2B, 0);
    ui_draw_string(buf, pitch, w, h, 24, 42,
                   "Keys: q=quit  r=redraw  m=move", 0xFFBFD6E6, 0xFF0F1E2B, 0);

    ui_fill_rect(buf, pitch, w, h, 30, 70, 60, 28, COL_RED);
    ui_fill_rect(buf, pitch, w, h, 96, 70, 60, 28, COL_GREEN);
    ui_fill_rect(buf, pitch, w, h, 162, 70, 60, 28, COL_BLUE);
    ui_fill_rect(buf, pitch, w, h, 228, 70, 60, 28, COL_YELLOW);

    ui_fill_rect(buf, pitch, w, h, 30, 110, w - 60, 6, COL_ACCENT);

    ui_fill_rect(buf, pitch, w, h, box_x, box_y, 24, 24, COL_ACCENT);
    ui_fill_rect(buf, pitch, w, h, box_x + 6, box_y + 6, 12, 12, 0xFF0B1220);

    if (last_mouse_x >= 0 && last_mouse_y >= 0) {
        int cx = last_mouse_x;
        int cy = last_mouse_y;
        ui_fill_rect(buf, pitch, w, h, cx - 8, cy, 16, 1, 0xFFFFFFFF);
        ui_fill_rect(buf, pitch, w, h, cx, cy - 8, 1, 16, 0xFFFFFFFF);
    }
}

static void render_wm(void) {
    render_scene(pixels, WIN_W, WIN_W, WIN_H);
    wm_flush(win_id, 0, 0, WIN_W, WIN_H);
}

static void step_anim(void) {
    box_x += box_dx;
    box_y += box_dy;
    if (box_x < 24 || box_x + 24 > WIN_W - 24) {
        box_dx = -box_dx;
        box_x += box_dx;
    }
    if (box_y < 120 || box_y + 24 > WIN_H - 24) {
        box_dy = -box_dy;
        box_y += box_dy;
    }
}

static int handle_event(const wm_event_t *evt) {
    if (evt->type == WM_EVENT_CLOSE) return 0;

    if (evt->type == WM_EVENT_MOUSE) {
        last_mouse_x = evt->x;
        last_mouse_y = evt->y;
        render_wm();
        return 1;
    }

    if (evt->type == WM_EVENT_KEY) {
        if (evt->button == 0) return 1;
        uint32_t key = evt->key;
        if (key == 'q' || key == 'Q' || key == 0x1B) return 0;
        if (key == 'r' || key == 'R') {
            render_wm();
        } else if (key == 'm' || key == 'M') {
            static int idx = 0;
            const int spots[4][2] = { {40, 40}, {140, 60}, {220, 100}, {90, 140} };
            idx = (idx + 1) & 3;
            wm_move_window(win_id, spots[idx][0], spots[idx][1]);
        }
    }

    return 1;
}

static int run_wm_mode(void) {
    if (wm_connect() != 0) {
        printf("[guitest] ERROR: wm_connect failed\n");
        return 1;
    }

    win_id = wm_create_window(WIN_X, WIN_Y, WIN_W, WIN_H, "GUI Test", &pixels);
    if (!win_id || !pixels) {
        printf("[guitest] ERROR: wm_create_window failed\n");
        wm_disconnect();
        return 1;
    }

    render_wm();

    uint32_t last_tick = syscall_get_ticks();
    while (1) {
        wm_event_t evt;
        if (wm_poll_event(&evt)) {
            if (!handle_event(&evt)) break;
        }

        uint32_t now = syscall_get_ticks();
        if (now != last_tick) {
            last_tick = now;
            step_anim();
            render_wm();
        }
        syscall_yield();
    }

    wm_destroy_window(win_id);
    wm_disconnect();
    return 0;
}

static int run_fb_mode(void) {
    uint32_t fb_w = 0, fb_h = 0, fb_pitch = 0;
    uint32_t *fb = (uint32_t *)syscall_fb_map(&fb_w, &fb_h, &fb_pitch);
    printf("[guitest] FB: %dx%d pitch=%d\n", fb_w, fb_h, fb_pitch);
    if (!fb) {
        printf("[guitest] ERROR: fb_map failed\n");
        return 1;
    }

    int pitch_px = (int)(fb_pitch / 4);

    /*
     * Tick-rate probe:
     * Wait for 100 observed tick changes and report elapsed syscall_get_ticks()
     * units. With a 100 Hz PIT and 10 ms units, this should be ~1000.
     */
    uint32_t tick_begin = syscall_get_ticks();
    uint32_t last_tick = tick_begin;
    uint32_t step_min = 0xFFFFFFFFu;
    uint32_t step_max = 0;
    uint32_t step_sum = 0;
    uint32_t steps = 0;

    while (steps < 100) {
        uint32_t now = syscall_get_ticks();
        if (now != last_tick) {
            uint32_t d = now - last_tick;
            if (d < step_min) step_min = d;
            if (d > step_max) step_max = d;
            step_sum += d;
            last_tick = now;
            steps++;
        }
        syscall_yield();
    }

    uint32_t tick_end = syscall_get_ticks();
    uint32_t elapsed = tick_end - tick_begin;
    uint32_t step_avg = (steps > 0) ? (step_sum / steps) : 0;
    printf("[guitest] tick-test: steps=%d elapsed=%d step[min=%d avg=%d max=%d]\n",
           (int)steps, (int)elapsed, (int)step_min, (int)step_avg, (int)step_max);

    /* Keep running in direct kernel launch mode so PID 1 does not exit. */
    while (1) {
        draw_gradient(fb, pitch_px, (int)fb_w, (int)fb_h);

        int bar_x = (int)(syscall_get_ticks() % (fb_w > 1 ? fb_w - 40 : 1));
        ui_fill_rect(fb, pitch_px, (int)fb_w, (int)fb_h, bar_x, 96, 40, 12, COL_ACCENT);

        ui_draw_string(fb, pitch_px, (int)fb_w, (int)fb_h,
                       16, 16, "RAW FRAMEBUFFER TEST", 0xFFFFFFFF, 0x00000000, 1);
        ui_draw_string(fb, pitch_px, (int)fb_w, (int)fb_h,
                       16, 30, "100 tick changes timing:", 0xFFFFFFFF, 0x00000000, 1);

        char elapsed_txt[32];
        char avg_txt[32];
        u32_to_str(elapsed, elapsed_txt, sizeof(elapsed_txt));
        u32_to_str(step_avg, avg_txt, sizeof(avg_txt));

        ui_draw_string(fb, pitch_px, (int)fb_w, (int)fb_h,
                       16, 44, "elapsed units:", 0xFFE6EEF6, 0x00000000, 1);
        ui_draw_string(fb, pitch_px, (int)fb_w, (int)fb_h,
                       16 + 14 * UI_FONT_W, 44, elapsed_txt, 0xFF4CC3FF, 0x00000000, 1);

        ui_draw_string(fb, pitch_px, (int)fb_w, (int)fb_h,
                       16, 58, "avg step:", 0xFFE6EEF6, 0x00000000, 1);
        ui_draw_string(fb, pitch_px, (int)fb_w, (int)fb_h,
                       16 + 10 * UI_FONT_W, 58, avg_txt, 0xFF4CC3FF, 0x00000000, 1);

        last_tick = syscall_get_ticks();
        // Wait for 1 second (100 ticks) before redrawing to avoid spamming the framebuffer with updates.
        while (syscall_get_ticks() <= last_tick + 10) {
            syscall_yield();
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && argv && argv[1] && strcmp(argv[1], "fb") == 0) {
        printf("[guitest] Starting in raw framebuffer mode\n");
        return run_fb_mode();
    }

    printf("[guitest] Starting in WM mode\n");
    return run_wm_mode();
}
