#include "kernel/process.h"
#include "drivers/pit.h"
#include "drivers/tty.h"
#include "kernel/io.h"
#include "kernel/system.h"

uint32_t tick = 0;
uint32_t timer_freq = 100;

void timer_callback(registers_t *regs) {
    // Called timer_freq Hz times per second (default 100 Hz or 100 ticks per second)
    tick++;

    // Switch to the next process every 10 ticks
    if (tick % 100 == 0) schedule(regs);
}

void sleep(uint32_t ms) {
	uint32_t start = tick;
	while ((uint32_t)(tick - start) < ms);
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