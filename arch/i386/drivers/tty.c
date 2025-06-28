#include "drivers/tty.h"
#include "drivers/serial.h"
#include "libc/string.h"
#include "kernel/io.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const uint16_t* VGA_MEMORY = (uint16_t*) 0xC00B8000;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

typedef enum {
    STATE_NORMAL,
    STATE_ESC,
    STATE_CSI
} ansi_state_t;

ansi_state_t ansi_state = STATE_NORMAL;

void update_cursor(size_t row, size_t col) 
{
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void terminal_initialize(void) 
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_buffer = VGA_MEMORY;
    
	terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_clear();
}

void terminal_setcolor(enum vga_color fg, enum vga_color bg) 
{
    terminal_color = fg | bg << 4;
}

uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
    return (uint16_t) uc | (uint16_t) color << 8;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    static char ansi_buffer[16];
    static int ansi_len = 0;

    switch (ansi_state) {
        case STATE_NORMAL:
            if (c == '\033') {
                ansi_state = STATE_ESC;
                ansi_len = 0;
            } else {
                // Handle regular characters
                serial_write(c);
                if (c == '\n') {
                    terminal_column = 0;
                    if (++terminal_row == VGA_HEIGHT) {
                        terminal_row--;
                        scroll_terminal();
                    }
                } else if (c == '\b') {
                    if (terminal_column == 0 && terminal_row > 0) {
                        terminal_row--;
                        terminal_column = VGA_WIDTH - 1;
                    } else if (terminal_column > 0) {
                        terminal_column--;
                    }

                    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
                } else {
                    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
                    if (++terminal_column == VGA_WIDTH) {
                        terminal_column = 0;
                        if (++terminal_row == VGA_HEIGHT) {
                            terminal_row--;
                            scroll_terminal();
                        }
                    }
                }
            }
            break;

        case STATE_ESC:
            if (c == '[') {
                ansi_state = STATE_CSI;
            } else {
                ansi_state = STATE_NORMAL;
            }
            break;

        case STATE_CSI:
            if ((c >= '0' && c <= '9') || c == ';') {
                if (ansi_len < sizeof(ansi_buffer) - 1) {
                    ansi_buffer[ansi_len++] = c;
                }
            } else {
                ansi_buffer[ansi_len] = '\0';

                if (c == 'J') { // Clear screen
                    if (ansi_buffer[0] == '2') {
                        terminal_clear(); // Clear entire screen
                    }
                } else if (c == 'H') { // Move cursor to 0,0
                    terminal_row = 0;
                    terminal_column = 0;
                } else if (c == '@') { // Insert character
                    for (size_t i = VGA_WIDTH - 1; i > terminal_column; i--) {
                        terminal_buffer[terminal_row * VGA_WIDTH + i] = terminal_buffer[terminal_row * VGA_WIDTH + i - 1];
                    }
                    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
                } else if (c == 'P') { // Delete character
                    for (size_t i = terminal_column; i < VGA_WIDTH - 1; i++) {
                        terminal_buffer[terminal_row * VGA_WIDTH + i] = terminal_buffer[terminal_row * VGA_WIDTH + i + 1];
                    }
                    terminal_putentryat(' ', terminal_color, VGA_WIDTH - 1, terminal_row);
                } else if (c == 'A') { // Move cursor up
                    if (terminal_row > 0) terminal_row--;
                } else if (c == 'B') { // Move cursor down
                    if (terminal_row < VGA_HEIGHT - 1) terminal_row++;
                } else if (c == 'C') { // Move cursor right
                    if (terminal_column < VGA_WIDTH - 1) terminal_column++;
                } else if (c == 'D') { // Move cursor left
                    if (terminal_column > 0) terminal_column--;
                } else {
                    // Unknown CSI sequence
                    log_to_serial("Unknown CSI sequence: ");
                    log_to_serial(ansi_buffer);
                    log_to_serial("\n");
                }

                ansi_state = STATE_NORMAL;
            }
            break;
    }

    update_cursor(terminal_row, terminal_column);
}

void terminal_write(const char* data, size_t size) 
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) 
{
	terminal_write(data, strlen(data));
}

void scroll_terminal() {
    // Move each line up by one
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    // Clear the last line
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
}


void terminal_clear() 
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    update_cursor(terminal_row, terminal_column);
}