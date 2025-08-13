#include "kernel/process.h"
#include "drivers/pit.h"
#include "drivers/tty.h"
#include "kernel/io.h"
#include "kernel/system.h"

#define SCHEDULER_FREQ 100 // 100 Hz or 10 ms per tick

uint32_t tick = 0;
uint32_t timer_freq = 100;
bool scheduler_enabled = true;

void timer_callback(registers_t *regs) {
    // Called timer_freq Hz times per second (default 100 Hz or 100 ticks per second or 10 ms per tick)
    tick++;

    // Switch to the next process every 10 ticks (or every 100 ms)
    if (tick % 1 == 0) schedule(regs);
    // printf("Tick: %d\n", tick);
    // schedule(regs);
}

void sleep(uint32_t ms) {
    if (!scheduler_enabled) return;

	uint32_t start = tick;
	while ((uint32_t)(tick - start) < ms / 10) {
        // Busy wait
        asm volatile("hlt"); // Halt the CPU to save power
    }
}

void init_timer(uint32_t freq) {
    register_interrupt_handler(32, &timer_callback);

    tick = 0;
    timer_freq = freq;
    uint32_t divisor = 1193180 / freq; // 1193180 Hz / freq Hz

    outb(0x43, 0x36); // Command port
    outb(0x40, divisor & 0xFF); // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
    scheduler_enabled = true;
}