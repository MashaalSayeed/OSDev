#include "kernel/io.h"
#include "drivers/mouse.h"
#include "kernel/printf.h"

#define MOUSE_IRQ 12

void ps2_mouse_wait(uint8_t type) {
    // type: 0 = data, 1 = signal
    int timeout = 100000;
    if (type == 0) {
        while (timeout-- && !(inb(PS2_MOUSE_CMD_PORT) & 1));
    } else {
        while (timeout-- && (inb(PS2_MOUSE_CMD_PORT) & 2));
    }
}

void ps2_mouse_write(uint8_t data) {
    ps2_mouse_wait(1);
    outb(PS2_MOUSE_CMD_PORT, 0xD4); // Command to write to mouse
    ps2_mouse_wait(1);
    outb(PS2_MOUSE_DATA_PORT, data);
}

uint8_t ps2_mouse_read() {
    ps2_mouse_wait(0); // Wait for data to be ready
    return inb(PS2_MOUSE_DATA_PORT);
}

static int mouse_x = 0, mouse_y = 0;
static uint8_t mouse_cycle = 0;
static uint8_t packet[3];
static mouse_state_t mouse_state;

mouse_state_t* get_mouse_state() {
    return &mouse_state;
}

// static int mouse_tick_counter = 0;
void mouse_irq_handler(registers_t *regs) {
    // mouse_tick_counter++;
    // if (mouse_tick_counter % 100 == 0) {
    //     printf("Mouse IRQ: %d\n", mouse_tick_counter);
    //     mouse_tick_counter = 0; // Reset counter every 100 ticks
    // }

    // Read mouse data
    packet[mouse_cycle++] = ps2_mouse_read();

    if (mouse_cycle == 3) {
        // Process complete mouse packet
        mouse_cycle = 0;
        int dx = packet[1];
        int dy = packet[2];
        if (packet[0] & 0x10) dx |= 0xFFFFFF00; // sign extend
        if (packet[0] & 0x20) dy |= 0xFFFFFF00;

        mouse_x += dx;
        mouse_y -= dy; // Invert Y

        // Clamp to screen bounds
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;

        mouse_state.x = mouse_x;
        mouse_state.y = mouse_y;
        mouse_state.dx = dx;
        mouse_state.dy = dy;
        mouse_state.left = (packet[0] & 0x01) != 0;
        mouse_state.right = (packet[0] & 0x02) != 0;
        mouse_state.middle = (packet[0] & 0x04) != 0;
    }
}

void ps2_mouse_init() {
    // Enable the auxiliary device (mouse)
    ps2_mouse_wait(1);
    outb(PS2_MOUSE_CMD_PORT, 0xA8);
    ps2_mouse_wait(1);
    outb(PS2_MOUSE_CMD_PORT, 0x20);
    ps2_mouse_wait(0);

    uint8_t status = inb(PS2_MOUSE_DATA_PORT);
    status |= 2; // Enable mouse

    ps2_mouse_wait(1);
    outb(PS2_MOUSE_CMD_PORT, 0x60);
    ps2_mouse_wait(1);
    outb(PS2_MOUSE_DATA_PORT, status);

    ps2_mouse_write(0xF6);
    ps2_mouse_read(); // Acknowledge command
    ps2_mouse_write(0xF4);
    ps2_mouse_read(); // Acknowledge command

    // Register mouse IRQ handler
    register_interrupt_handler(IRQ_BASE + MOUSE_IRQ, mouse_irq_handler);
}