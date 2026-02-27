#pragma once

#include <stdint.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

void initialize_lock(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

/* IRQ-save variants (save/restore EFLAGS around lock acquisition).
 * Use these when callers may run in IRQ context or when you must
 * prevent interrupts from occurring while holding the lock.
 */
void spinlock_acquire_irq(spinlock_t *lock, uint32_t *flags_out);
void spinlock_release_irq(spinlock_t *lock, uint32_t flags);