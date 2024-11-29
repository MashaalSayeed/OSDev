#include "kernel/timer.h"
#include "kernel/process.h"
#include "drivers/tty.h"
#include "libc/stdio.h"
#include "io.h"
#include "system.h"

uint32_t tick = 0;
uint32_t timer_freq = 100;

void timer_callback(registers_t *regs) {
    tick++;
    schedule(regs);
}

void init_timer(uint32_t freq) {
    register_interrupt_handler(32, &timer_callback);

    tick = 0;
    timer_freq = freq;
    uint32_t divisor = 1193180 / freq; // 1193180 Hz / freq Hz

    outb(0x43, 0x36); // Command port
    outb(0x40, divisor & 0xFF); // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
}