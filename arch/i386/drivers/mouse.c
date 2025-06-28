#include "kernel/io.h"
#include "drivers/mouse.h"
#include "kernel/printf.h"
#include "kernel/isr.h"


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
    ps2_mouse_wait(1); // Wait for command port to be ready
    outb(PS2_MOUSE_CMD_PORT, 0xD4); // Command to write to mouse
    ps2_mouse_wait(1);
    outb(PS2_MOUSE_DATA_PORT, data);
}

uint8_t ps2_mouse_read() {
    ps2_mouse_wait(0); // Wait for data to be ready
    return inb(PS2_MOUSE_DATA_PORT);
}

static int8_t mouse_x = 0, mouse_y = 0;
static uint8_t mouse_cycle = 0;
static uint8_t packet[3];

void mouse_irq_handler(registers_t *regs) {
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

        printf("Mouse Position: X: %d, Y: %d\n", mouse_x, mouse_y);
    }
}

void ps2_mouse_init() {
    // Enable the auxiliary device (mouse)
    outb(PS2_MOUSE_CMD_PORT, 0xA8);
    outb(PS2_MOUSE_CMD_PORT, 0x20);

    uint8_t status = inb(PS2_MOUSE_DATA_PORT);
    status |= 2; // Enable mouse

    outb(PS2_MOUSE_CMD_PORT, 0x60);
    outb(PS2_MOUSE_DATA_PORT, status);

    ps2_mouse_write(0xF6);
    ps2_mouse_read(); // Acknowledge command
    ps2_mouse_write(PS2_MOUSE_ENABLE);
    ps2_mouse_read(); // Acknowledge command

    // Register mouse IRQ handler
    register_interrupt_handler(12, mouse_irq_handler);
}