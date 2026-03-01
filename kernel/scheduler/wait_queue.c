#include "kernel/wait_queue.h"
#include "kernel/process.h"
#include "kernel/printf.h"


// Adds the current thread to the wait queue and marks it as WAITING, then yields.
// Entry is allocated on the caller's kernel stack in wait_queue_sleep —
// no heap allocation needed, no kmalloc, no fragmentation.
void wait_queue_sleep(wait_queue_t *q) {
    wait_queue_entry_t entry = {
        .thread = get_current_thread(),
        .next   = NULL,
    };

    // Append to tail (FIFO wake order)
    if (!q->head) {
        q->head = &entry;
    } else {
        wait_queue_entry_t *tail = q->head;
        while (tail->next) tail = tail->next;
        tail->next = &entry;
    }

    // Mark thread as waiting and yield
    // schedule() will not pick this thread again until status = READY
    entry.thread->status = WAITING;
    schedule(NULL);

    // When we return here the waker has already set status = READY
    // and removed us from the queue. The caller re-checks its condition.
}

void wait_queue_wake(wait_queue_t *q) {
    if (!q->head) return;

    wait_queue_entry_t *entry = q->head;
    q->head = entry->next;

    entry->thread->status = READY;
    // entry itself lives on the sleeping thread's kernel stack — no free needed
}

void wait_queue_wake_all(wait_queue_t *q) {
    while (q->head) {
        wait_queue_wake(q);
    }
}