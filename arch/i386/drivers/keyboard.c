#include "drivers/keyboard.h"
#include "drivers/tty.h"
#include "drivers/serial.h"
#include "libc/string.h"
#include "kernel/isr.h"
#include "kernel/io.h"
#include "kernel/system.h"
#include "kernel/printf.h"

#define KEYBOARD_DATA_PORT 0x60

const uint32_t UNKNOWN = 0xFFFFFFFF;
const uint32_t ESC = 0xFFFFFFFF - 1;
const uint32_t CTRL = 0xFFFFFFFF - 2;
const uint32_t LSHFT = 0xFFFFFFFF - 3;
const uint32_t RSHFT = 0xFFFFFFFF - 4;
const uint32_t ALT = 0xFFFFFFFF - 5;
const uint32_t ENTER = 0x0D;
const uint32_t F1 = 0xFFFFFFFF - 6;
const uint32_t F2 = 0xFFFFFFFF - 7;
const uint32_t F3 = 0xFFFFFFFF - 8;
const uint32_t F4 = 0xFFFFFFFF - 9;
const uint32_t F5 = 0xFFFFFFFF - 10;
const uint32_t F6 = 0xFFFFFFFF - 11;
const uint32_t F7 = 0xFFFFFFFF - 12;
const uint32_t F8 = 0xFFFFFFFF - 13;
const uint32_t F9 = 0xFFFFFFFF - 14;
const uint32_t F10 = 0xFFFFFFFF - 15;
const uint32_t F11 = 0xFFFFFFFF - 16;
const uint32_t F12 = 0xFFFFFFFF - 17;
const uint32_t SCRLCK = 0xFFFFFFFF - 18;
const uint32_t HOME = 0xFFFFFFFF - 19;
const uint32_t UP = 0xFFFFFFFF - 20;
const uint32_t LEFT = 0xFFFFFFFF - 21;
const uint32_t RIGHT = 0xFFFFFFFF - 22;
const uint32_t DOWN = 0xFFFFFFFF - 23;
const uint32_t PGUP = 0xFFFFFFFF - 24;
const uint32_t PGDOWN = 0xFFFFFFFF - 25;
const uint32_t END = 0xFFFFFFFF - 26;
const uint32_t INS = 0xFFFFFFFF - 27;
const uint32_t DEL = 0xFFFFFFFF - 28;
const uint32_t CAPS = 0xFFFFFFFF - 29;
const uint32_t NONE = 0xFFFFFFFF - 30;
const uint32_t ALTGR = 0xFFFFFFFF - 31;
const uint32_t NUMLCK = 0xFFFFFFFF - 32;

const uint32_t keyboard_map[128] = {
    UNKNOWN,ESC,'1','2','3','4','5','6','7','8',
    '9','0','-','=','\b','\t','q','w','e','r',
    't','y','u','i','o','p','[',']','\n',CTRL,
    'a','s','d','f','g','h','j','k','l',';',
    '\'','`',LSHFT,'\\','z','x','c','v','b','n','m',',',
    '.','/',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',LEFT,UNKNOWN,RIGHT,
    '+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

const uint32_t keyboard_map_shift[128] = {
    UNKNOWN,ESC,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R',
    'T','Y','U','I','O','P','{','}','\n',CTRL,'A','S','D','F','G','H','J','K','L',':','"','~',LSHFT,'|','Z','X','C',
    'V','B','N','M','<','>','?',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',
    LEFT,UNKNOWN,RIGHT,'+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};


static int shift_on, ctrl_on, alt_on;
static int caps_lock;

#define BUFFER_SIZE 256
char keyboard_buffer[BUFFER_SIZE];

size_t buffer_start, buffer_end;

void keyboard_callback(registers_t *regs) {
    unsigned char scancode = inb(KEYBOARD_DATA_PORT);
    
    if (scancode & 0x80) {
        // Key released
        scancode &= 0x7F;
        if (scancode == 42 || scancode == 54) shift_on = 0;
        if (scancode == 29 || scancode == 56) ctrl_on = 0;
        return;
    }

    if (scancode == 42 || scancode == 54) {
        shift_on = 1;
        return;
    }

    if (scancode == 29 || scancode == 56) {
        ctrl_on = 1;
        return;
    }

    if (scancode == 58) {
        caps_lock = !caps_lock;
        return;
    }

    switch (scancode) {
        case 0x48: // Up arrow
            keyboard_buffer[buffer_end] = '\033';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = '[';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = 'A'; // Up
            break;
        case 0x50: // Down arrow
            keyboard_buffer[buffer_end] = '\033';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = '[';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = 'B'; // Up
            break;
        case 0x4B: // Left arrow
            keyboard_buffer[buffer_end] = '\033';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = '[';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = 'D'; // Left
            break;
        case 0x4D: // Right arrow
            keyboard_buffer[buffer_end] = '\033';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = '[';
            buffer_end = (buffer_end + 1) % BUFFER_SIZE;
            keyboard_buffer[buffer_end] = 'C'; // Right
            break;
        default:
            // if (scancode >= 128) return;
            char c = (shift_on || caps_lock) ? keyboard_map_shift[scancode] : keyboard_map[scancode];
            if (c && c != UNKNOWN) {
                if (ctrl_on && keyboard_map[scancode] >= 'a' && keyboard_map[scancode] <= 'z') {
                    c = c & 0x1F; // Convert to control character
                }

                keyboard_buffer[buffer_end] = c;
                if (buffer_end >= BUFFER_SIZE) {
                    buffer_end = 0;
                }
            }
            break;
    }

    // terminal_putchar(c);
    buffer_end = (buffer_end + 1) % BUFFER_SIZE;
    UNUSED(regs);
}

char kgetch() {
    asm volatile("sti");
    while (buffer_start == buffer_end) {
        // Wait for a key press
    }

    char c = keyboard_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % BUFFER_SIZE;
    return c;
}

int kgets(char *buffer, size_t size) {
    size_t i = 0;
    while (i < size) {
        char c = kgetch();
        if (c == '\n') {
            buffer[i] = '\0';
            return i + 1;
        } else if (c == '\b' && i > 0) {
            i--;
        } else if (c != '\b') {
            buffer[i++] = c;
        }
    }

    buffer[i] = '\0';
    return i + 1;
}

size_t read_keyboard_buffer(uint8_t *buf, size_t size) {
    size_t count = 0;
    while (count < size) {
        buf[count] = kgetch();
        count++;
    }
    return count;
}

void init_keyboard() {
    shift_on = 0;
    caps_lock = 0;
    buffer_start = 0;
    buffer_end = 0;

    register_interrupt_handler(33, &keyboard_callback);
}