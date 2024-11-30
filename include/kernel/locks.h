#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool lock;
} spinlock_t;

void initialize_lock(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);