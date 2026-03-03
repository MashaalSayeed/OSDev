#include "kernel/locks.h"
#include "kernel/kheap.h"

void spinlock_init(spinlock_t *lock) {
    lock->lock = 0;
}

void spinlock_acquire_irq(spinlock_t *lock, uint32_t *flags_out) {
    uint32_t flags;
    /* Save EFLAGS (including IF) and disable interrupts */
    asm volatile ("pushfl; pop %0" : "=r" (flags) :: "memory");
    asm volatile ("cli" ::: "memory");
    spinlock_acquire(lock);
    if (flags_out) *flags_out = flags;
}

void spinlock_release_irq(spinlock_t *lock, uint32_t flags) {
    spinlock_release(lock);
    /* Restore EFLAGS (may re-enable interrupts) */
    asm volatile ("push %0; popfl" :: "r" (flags) : "memory");
}