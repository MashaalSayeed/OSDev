#pragma once
/*
 * input.h — Kernel input event queue.
 *
 * The mouse and keyboard drivers push events into this queue, and the
 * compositor thread pops them out to forward to clients via /DEV/WM.
 *
 * This is a simple circular buffer; if the queue is full, new events are
 * dropped on the floor.  The compositor should keep up with the event rate
 * under normal circumstances, so this shouldn't be a problem.
 */

#include "kernel/vfs.h"

#define INPUT_QUEUE_SIZE 128

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

typedef struct {
    input_event_t buf[INPUT_QUEUE_SIZE];
    int head;
    int tail;
} input_queue_t;

/* queue ops (non-blocking; swap in wait/wakeup later) */
void input_queue_init(input_queue_t *q);
int  input_queue_push(input_queue_t *q, const input_event_t *evt); /* returns 1 on success, 0 if full */
int  input_queue_pop(input_queue_t *q, input_event_t *out);         /* returns 1 on success, 0 if empty */

/* generic devfs char callbacks that use device->device_data as input_queue_t* */
int input_dev_read(vfs_device_t *dev, char *buf, size_t count);
int input_dev_write(vfs_device_t *dev, const char *buf, size_t count);
