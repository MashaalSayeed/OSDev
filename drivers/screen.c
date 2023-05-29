#include "screen.h"
#include "ports.h"
#include "../kernel/util.h"

#define VIDEO_ADDRESS 0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80

// Screen device I/O ports 
#define REG_SCREEN_CTRL 0x3D4 
#define REG_SCREEN_DATA 0x3D5

char attrib = 0x0f;

int get_screen_offset(int row, int col) { 
    return 2 * (row * MAX_COLS + col); 
}

int get_offset_row(int offset) {
    return offset / (2 * MAX_COLS);
}

int get_offset_col(int offset) {
    return (offset - (get_offset_row(offset)* 2 * MAX_COLS)) / 2;
}

int get_cursor_offset() {
    // Requesting byte 14: high byte of cursor pos
    port_byte_out(REG_SCREEN_CTRL, 14);
    int position = port_byte_in(REG_SCREEN_DATA) << 8;

    // Requesting byte 15: lower byte of cursor pos
    port_byte_out(REG_SCREEN_CTRL, 15);
    position += port_byte_in(REG_SCREEN_DATA);
    return position * 2;
}

void set_cursor_offset(int offset) {
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

void set_color(unsigned char forecolor, unsigned char backcolor) {
    attrib = (backcolor << 4) | (forecolor & 0x0F);
}

int handle_scrolling(int offset) {
    char* screen = (char *) VIDEO_ADDRESS;
    if (offset < 2 * MAX_ROWS * MAX_COLS) return offset;

    for (int i = 1; i < MAX_ROWS; i++) {
        // Copy ith row to i-1th row
        memory_copy(
            screen + get_screen_offset(i, 0), 
            screen + get_screen_offset(i-1, 0), 
            MAX_COLS
        );
    }

    char* last_line = screen + get_screen_offset(MAX_ROWS -1, 0); 
    for (int i = 0; i < MAX_COLS*2; i++) {
        last_line[i] = 0; 
    }

    offset -= 2 * MAX_COLS;
    return offset;
}

/* Innermost print function */
int print_char_at(char character, int row, int col) {
    char* screen = (char *) VIDEO_ADDRESS;

    int offset;
    if (row >= 0 && col >= 0) {
        offset = get_screen_offset(row, col);
    } else {
        offset = get_cursor_offset();
    }

    if (character == '\n') {
        row = get_offset_row(offset);
        offset = get_screen_offset(row + 1, 0);
    } else {
        screen[offset] = character;
        screen[offset+1] = attrib;
        offset += 2;
    }

    offset = handle_scrolling(offset);
    set_cursor_offset(offset);
    return offset;
}

int print_char(char character) {
    return print_char_at(character, -1, -1);
}

void print_string_at(char *message, int row, int col) {
    for (int i = 0; message[i] != '\0'; i++) {
        int offset = print_char_at(message[i], row, col);
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}

void print_string(char *message) {
    print_string_at(message, -1, -1);
}

void clear_screen() {
    int screen_size = MAX_ROWS * MAX_COLS;
    char *screen = (char *) VIDEO_ADDRESS;
    for (int i = 0; i < screen_size; i++) {
        screen[i*2] = ' ';
        screen[i*2+1] = WHITE_ON_BLACK;
    }

    set_cursor_offset(0);
}