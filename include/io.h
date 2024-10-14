#pragma once

#include <stdint.h>

#define UNUSED(x) (void)(x)

static inline void outb(uint16_t port, uint8_t value) {
    // __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
    asm volatile ("outb %1, %0" : : "dN"(port), "a"(value));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}