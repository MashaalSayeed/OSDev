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