#include "drivers/tty.h"
#include "libc/string.h"
#include "io.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xC00B8000;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

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
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}

    update_cursor(terminal_row, terminal_column);
}

void terminal_setcolor(uint8_t color) 
{
    terminal_color = color;
}

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
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

void terminal_putchar(char c) 
{
    switch (c)
    {
    case '\n':
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_row = 0;
        break;
    case '\b':
        if (terminal_column == 0 && terminal_row != 0){
            terminal_row--;
            terminal_column = VGA_WIDTH;
        }
        terminal_buffer[terminal_row * VGA_WIDTH + (--terminal_column)] = ' ' | terminal_color;
        break;
    case '\t':
        terminal_column += 4;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT)
                terminal_row = 0;
        }
        break;
    default:
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT)
                terminal_row = 0;
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