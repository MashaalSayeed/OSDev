#pragma once

// #include "kernel/process.h"
#include <stdbool.h>
#include <stddef.h>

struct thread; // Forward declaration to avoid circular dependency

typedef struct wait_queue_entry {
    struct thread *thread;
    struct wait_queue_entry *next;
} wait_queue_entry_t;

typedef struct {
    wait_queue_entry_t *head;
} wait_queue_t;

static inline void wait_queue_init(wait_queue_t *queue) {
    queue->head = NULL;
}

void wait_queue_sleep(wait_queue_t *queue);
void wait_queue_wake(wait_queue_t *queue);
void wait_queue_wake_all(wait_queue_t *queue);

static inline bool wait_queue_empty(wait_queue_t *queue) {
    return queue->head == NULL;
}