#include "kernel/gui.h"

#define MAX_EVENTS 128

static input_event_t event_queue[MAX_EVENTS];
static unsigned int event_head = 0;
static unsigned int event_tail = 0;

void input_push_event(const input_event_t *evt) {
    unsigned int next = (event_tail + 1) % MAX_EVENTS;
    if (next == event_head) {
        // Queue is full, cannot push new event
        return;
    }

    event_queue[event_tail] = *evt;
    event_tail = next;
}

int input_pop_event(input_event_t *evt) {
    if (event_head == event_tail) return 0;

    *evt = event_queue[event_head];
    event_head = (event_head + 1) % MAX_EVENTS;
    return 1;
}