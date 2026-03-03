#pragma once

#include <stdint.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

void spinlock_init(spinlock_t *lock);

static inline void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->lock, 1)) {
        while (lock->lock) {
            asm volatile ("pause" ::: "memory");
        }
    }
}

static inline void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->lock);
}

/* IRQ-save variants (save/restore EFLAGS around lock acquisition).
 * Use these when callers may run in IRQ context or when you must
 * prevent interrupts from occurring while holding the lock.
 */
void spinlock_acquire_irq(spinlock_t *lock, uint32_t *flags_out);
void spinlock_release_irq(spinlock_t *lock, uint32_t flags);