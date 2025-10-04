#include "user/term.h"
#include "user/stdio.h"
#include "user/syscall.h"
#include "libc/string.h"
#include <stddef.h>

#define MAX_INPUT 256
#define MAX_HISTORY 10

static char history_buffer[MAX_HISTORY][MAX_INPUT];
static int history_pos = 0;
static int history_count = 0;

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

void history_add(const char *command) {
    if (!command || command[0] == '\0') return; // Ignore empty commands
    if (history_count > 0 && strcmp(command, history_buffer[(history_count - 1) % MAX_HISTORY]) == 0) {
        return; // Ignore duplicate of last command
    }

    strncpy(history_buffer[history_count % MAX_HISTORY], command, MAX_INPUT - 1);
    history_buffer[history_count % MAX_HISTORY][MAX_INPUT - 1] = '\0';
    history_count++;
    history_pos = history_count;
}

const char* history_pop(int direction) {
    if (direction == -1 && history_pos > 0) { // Up
        history_pos--;
    } else if (direction == 1 && history_pos < history_count) { // Down
        history_pos++;
    }

    if (history_pos >= 0 && history_pos < history_count) {
        return history_buffer[history_pos % MAX_HISTORY];
    }
    return "";
}

int history_print() {
    int start = (history_count > MAX_HISTORY) ? (history_count - MAX_HISTORY) : 0;
    for (int i = start; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history_buffer[i % MAX_HISTORY]);
    }
    return history_count - start;
}

int readline(char *buffer, int size) {
    if (size <= 1 || buffer == NULL) return -1; // Invalid parameters

    int cur = 0;
    int length = 0;
    char c;

    buffer[0] = '\0';
    history_pos = history_count;

    while (1) {
        c = getch();

        if (c == '\n' || c == '\r') {
            buffer[length] = '\0';
            printf("\n");
            history_add(buffer);
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
                    const char *hist_cmd = history_pop(-1);
                    if (hist_cmd) {
                        printf("\033[%dD", cur); // Move cursor to start
                        printf("\033[0K");       // Clear line from cursor

                        strncpy(buffer, hist_cmd, size);
                        buffer[size - 1] = '\0';
                        length = cur = strlen(buffer);
                        printf("%s", buffer);
                    }
                } else if (seq[1] == 'B') { // Down arrow
                    const char *hist_cmd = history_pop(1);
                    if (hist_cmd) {
                        printf("\033[%dD", cur); // Move cursor to start
                        printf("\033[0K");       // Clear line from cursor

                        strncpy(buffer, hist_cmd, size);
                        buffer[size - 1] = '\0';
                        length = cur = strlen(buffer);
                        printf("%s", buffer);
                    }
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
