#include "kernel/io.h"
#include "drivers/mouse.h"
#include "kernel/isr.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/wm_dev.h"

#define MOUSE_IRQ 12
#define PACKET_SIZE 3

static int mouse_x = 0, mouse_y = 0;
static uint8_t mouse_cycle = 0;
static uint8_t packet[PACKET_SIZE];
// static mouse_state_t mouse_state;
// static input_queue_t mouse_queue;

// mouse_state_t* get_mouse_state() {
//     return &mouse_state;
// }

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

// static bool prev_left = false;
// static bool prev_right = false;
// static bool prev_middle = false;

void mouse_irq_handler(registers_t *regs) {
    uint8_t data = ps2_mouse_read();

    if (mouse_cycle == 0 && !(data & 0x08)) {
        // Ignore if the first byte does not have the mouse data bit set
        return;
    }

    packet[mouse_cycle++] = data;
    if (mouse_cycle >= PACKET_SIZE) {
        // Process complete mouse packet
        mouse_cycle = 0;
        int dx = (int8_t) packet[1];
        int dy = (int8_t) packet[2];

        if (packet[0] & 0x40) dx = (dx < 0) ? -255 : 255; // X overflow
        if (packet[0] & 0x80) dy = (dy < 0) ? -255 : 255; // Y overflow

        mouse_x += dx;
        mouse_y -= dy; // Invert Y

        // Clamp to screen bounds
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;


        // Bit 0: Left, Bit 1: Right, Bit 2: Middle
        uint32_t buttons = packet[0] & 0x07;

        wm_dev_push_input_event(
            WM_EVENT_MOUSE, 
            mouse_x, 
            mouse_y, 
            0,       // Keycode (unused for mouse)
            buttons  // Button mask
        );
        // bool left = packet[0] & 0x01;
        // bool right = packet[0] & 0x02;
        // bool middle = packet[0] & 0x04;

        // unit32_t buttons;

        // input_queue_push(&mouse_queue, &evt);
        // wm_dev_push_input_event(WM_EVENT_MOUSE, mouse_x, mouse_y, 0, 0);
        // if (left && !prev_left) {
        //     evt.mouse.type = MOUSE_EVENT_DOWN;
        //     evt.mouse.button = 0; // Left button
        //     input_queue_push(&mouse_queue, &evt);
        // } else if (!left && prev_left) {
        //     evt.mouse.type = MOUSE_EVENT_UP;
        //     evt.mouse.button = 0; // Left button
        //     input_queue_push(&mouse_queue, &evt);
        // }

        // if (right && !prev_right) {
        //     evt.mouse.type = MOUSE_EVENT_DOWN;
        //     evt.mouse.button = 1; // Right button
        //     input_queue_push(&mouse_queue, &evt);
        // } else if (!right && prev_right) {
        //     evt.mouse.type = MOUSE_EVENT_UP;
        //     evt.mouse.button = 1; // Right button
        //     input_queue_push(&mouse_queue, &evt);
        // }

        // if (middle && !prev_middle) {
        //     evt.mouse.type = MOUSE_EVENT_DOWN;
        //     evt.mouse.button = 2; // Middle button
        //     input_queue_push(&mouse_queue, &evt);
        // } else if (!middle && prev_middle) {
        //     evt.mouse.type = MOUSE_EVENT_UP;
        //     evt.mouse.button = 2; // Middle button
        //     input_queue_push(&mouse_queue, &evt);
        // }

        // prev_left = left;
        // prev_right = right;
        // prev_middle = middle;
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

    // input_queue_init(&mouse_queue);

    // static vfs_device_t mouse_dev;
    // memset(&mouse_dev, 0, sizeof(mouse_dev));
    // strncpy(mouse_dev.name, "MOUSE", DEVFS_NAME_LEN - 1);
    // mouse_dev.type = DEVICE_TYPE_CHAR;
    // mouse_dev.device_data = &mouse_queue;
    // mouse_dev.char_dev.read_char  = input_dev_read;
    // mouse_dev.char_dev.write_char = input_dev_write;

    // if (devfs_register_device(&mouse_dev) == 0)
    //     kprintf(INFO, "mouse_dev: registered /DEV/MOUSE\n");
    // else
    //     kprintf(ERROR, "mouse_dev: failed to register /DEV/MOUSE\n");
}