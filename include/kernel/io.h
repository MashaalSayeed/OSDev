#pragma once

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %1, %0" : : "dN"(port), "a"(value));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

// Read 32-bit value from an I/O port
static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Write 32-bit value to an I/O port
static inline void outl(uint16_t port, uint32_t data) {
    __asm__ volatile ("outl %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile ("outw %1, %0" : : "dN"(port), "a"(value));
}