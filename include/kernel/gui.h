#pragma once

#include "kernel/framebuffer.h"
#include <stdint.h>

typedef enum {
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_KEYBOARD,
    // extendable
} input_type_t;

typedef enum {
    MOUSE_EVENT_MOVE,
    MOUSE_EVENT_DOWN,
    MOUSE_EVENT_UP,
    MOUSE_EVENT_SCROLL
} mouse_event_type_t;

typedef struct {
    mouse_event_type_t type;
    int x, y;
    int dx, dy;
    int button; // 0 = left, 1 = right, 2 = middle
} mouse_event_t;

typedef struct {
    input_type_t type;
    union {
        mouse_event_t mouse;
        // keyboard_event_t key; etc...
    };
} input_event_t;

void input_push_event(const input_event_t *evt);
int input_pop_event(input_event_t *evt);

void handle_mouse_event(mouse_event_t *evt);
void cursor_save_bg(unsigned int x, unsigned int y);
void cursor_restore_bg();
void cursor_draw();

void gui_init(framebuffer_t *fb_data);
void gui_loop();
