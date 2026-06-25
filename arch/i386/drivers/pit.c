#include "kernel/process.h"
#include "drivers/pit.h"
#include "drivers/tty.h"
#include "kernel/io.h"
#include "kernel/isr.h"
#include "kernel/system.h"
#include "kernel/printf.h"

#define SCHEDULER_FREQ 100 // 100 Hz or 10 ms per tick

static volatile uint32_t tick = 0;
uint32_t timer_freq = 100;
extern thread_t* current_thread;

void pit_handler(registers_t *regs) {
    // Send EOI to the PIC *first*, before any scheduling.
    // schedule() → switch_task() can divert execution to a new thread via
    // a bare `ret` without ever returning here, so the EOI in irq_handler
    // would never be sent — causing the PIC to stop delivering IRQ0.
    outb(0x20, 0x20);

    tick++;

    if (current_thread) {
        if (current_thread->time_slice_remaining > 0) {
            current_thread->time_slice_remaining--;
        }
        if (current_thread->time_slice_remaining == 0) {
            schedule(regs);
        }
    }
}

uint32_t pit_get_ticks() {
    return tick;
}

void sleep(uint32_t ms) {
    uint32_t ticks_needed = (ms * timer_freq) / 1000;
    current_thread->wakeup_tick = tick + ticks_needed;
    current_thread->status = SLEEPING;
    schedule(NULL);
}

void pit_init(uint32_t freq) {
    register_interrupt_handler(32, &pit_handler);

    tick = 0;
    timer_freq = freq;
    uint32_t divisor = 1193180 / freq; // 1193180 Hz / freq Hz

    outb(0x43, 0x36); // Command port
    outb(0x40, divisor & 0xFF); // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
}