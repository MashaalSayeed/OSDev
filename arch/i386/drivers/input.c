#include <drivers/input.h>
#include <kernel/printf.h>
#include <libc/string.h>

input_queue_t mouse_queue;
input_queue_t keyboard_queue;

void input_queue_init(input_queue_t *q) {
    if (!q) return;
    memset(q, 0, sizeof(*q));
}

int input_queue_push(input_queue_t *q, const input_event_t *evt) {
    if (!q || !evt) return 0;
    uint32_t next = (q->tail + 1) % INPUT_QUEUE_SIZE;
    if (next == q->head) return 0; /* full */
    q->buf[q->tail] = *evt;
    q->tail = next;
    /* TODO: wake readers (waitqueue) */
    return 1;
}

int input_queue_pop(input_queue_t *q, input_event_t *out) {
    if (!q || !out) return 0;
    if (q->head == q->tail) return 0; /* empty */
    *out = q->buf[q->head];
    q->head = (q->head + 1) % INPUT_QUEUE_SIZE;
    return 1;
}

/* devfs char callbacks (use device->device_data as input_queue_t*) */
int input_dev_read(vfs_device_t *dev, char *buf, size_t count) {
    if (!dev || !buf) return -1;
    input_queue_t *q = (input_queue_t*)dev->device_data;
    input_event_t evt;
    if (count < sizeof(evt)) return -1;
    if (!input_queue_pop(q, &evt)) return 0; /* empty -> non-blocking */
    memcpy(buf, &evt, sizeof(evt));
    return (int)sizeof(evt);
}

int input_dev_write(vfs_device_t *dev, const char *buf, size_t count) {
    (void)dev; (void)buf; (void)count;
    return -1; /* no writes supported here (use ioctls later) */
}