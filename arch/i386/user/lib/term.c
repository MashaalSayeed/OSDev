#include "user/term.h"
#include "user/stdio.h"
#include "user/syscall.h"
#include <stddef.h>

#define MAX_INPUT 256
#define MAX_HISTORY 10

void term_move_cursor(int row, int col) {
    if (row < 0 || col < 0) return; // Invalid position
    printf("\033[%d;%dH", row + 1, col + 1);
}

void term_clear_screen() {
    printf("\033[2J");
    term_move_cursor(0, 0);
}

void term_set_text_color(int color) {
    printf("\033[38;5;%dm", color);
}

void term_reset_text_color() {
    printf("\033[0m"); // Reset to default text color
}

void term_get_size(int *rows, int *cols) {
    if (rows == NULL || cols == NULL) return; // Invalid parameters

    *rows = 25; // Default terminal rows
    *cols = 80; // Default terminal columns
}

int readline(char *buffer, int size) {
    if (size <= 0 || buffer == NULL) return -1; // Invalid parameters

    int cur = 0;
    int length = 0;
    char c;

    while (1) {
        c = getch();

        if (c == '\n' || c == '\r') {
            buffer[length] = '\0';
            printf("\n");
            break;
        } else if (c == '\b' || c == 127) { // Backspace
            if (cur > 0) {
                cur--;
                length--;
                for (int i = cur; i < length; i++) {
                    buffer[i] = buffer[i + 1];
                }
                buffer[length] = '\0';
                printf("\033[D\033[P"); // Move cursor back and clear character
            }
        } else if (c == '\033') { // Escape sequence
            char seq[2];
            if (syscall_read(STDIN, seq, 2) < 0) return -1;
            if (seq[0] == '[') {
                if (seq[1] == 'D' && cur > 0) { // Left arrow
                    cur--;
                    printf("\033[D");
                } else if (seq[1] == 'C' && cur < length) { // Right arrow
                    cur++;
                    printf("\033[C");
                } else if (seq[1] == 'A') { // Up arrow
                    // Load previous history entry (not implemented here)
                } else if (seq[1] == 'B') { // Down arrow
                    // Load next history entry (not implemented here)
                }
            }
        } else {
            if (length < size - 1) {
                for (int i = length; i >= cur; i--) {
                    buffer[i + 1] = buffer[i];
                }

                buffer[cur++] = c;
                length++;
                printf("\033[@"); // Insert space at cursor
                printf("%c", c);  // Write the new character
            }
        }
    }

    return length;
}