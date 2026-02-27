#include "kernel/locks.h"
#include "kernel/kheap.h"

void initialize_lock(spinlock_t *lock) {
    lock->lock = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->lock, 1));
}

void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->lock);
}

void spinlock_acquire_irq(spinlock_t *lock, uint32_t *flags_out) {
    uint32_t flags;
    /* Save EFLAGS (including IF) and disable interrupts */
    asm volatile ("pushfl; pop %0" : "=r" (flags) :: "memory");
    asm volatile ("cli" ::: "memory");
    while (__sync_lock_test_and_set(&lock->lock, 1)) ;
    if (flags_out) *flags_out = flags;
}

void spinlock_release_irq(spinlock_t *lock, uint32_t flags) {
    __sync_lock_release(&lock->lock);
    /* Restore EFLAGS (may re-enable interrupts) */
    asm volatile ("push %0; popfl" :: "r" (flags) : "memory");
}