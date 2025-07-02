#include "libc/string.h"
#include "user/stdio.h"
#include "user/syscall.h"
#include "user/term.h"

#define MAX_LINES 100
#define MAX_COLS 80

char *filename;
char text[MAX_LINES][MAX_COLS];
int running = 1;
int line_count = 0;
int cursor_x = 0, cursor_y = 0;
int scroll_offset = 0;

int term_rows, term_cols;

void redraw_screen() {
    term_clear_screen();
    for (int i = 0; i < line_count; i++) {
        printf("%s\n", text[i]);
    }

    term_move_cursor(term_rows - 1, 0);
    printf("[Ctrl-X to exit] [Ctrl-S to save] [Line %d, Col %d]", cursor_y + 1, cursor_x + 1);
    term_move_cursor(cursor_y, cursor_x);
}

void editor_load_file(const char *filename) {
    int fd;
}

void editor_save_file(const char *filename) {
    int fd = syscall_open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("Error: Failed to open file for writing\n");
        return;
    }

    for (int i = 0; i < line_count; i++) {
        syscall_write(fd, text[i], strlen(text[i]));
        syscall_write(fd, "\n", 1); // Write newline after each line
    }

    syscall_close(fd);
    printf("\nFile saved successfully.\n");
}

void handle_input(char c) {
    if (c == '\033') { // Escape sequence
        getch(); // Skip the '[' character
        switch (getch()) { // Get the next character
            case 'A': // Up arrow
                if (cursor_y > 0) cursor_y--;
                if (cursor_x > strlen(text[cursor_y])) cursor_x = strlen(text[cursor_y]); 
                break;
            case 'B': // Down arrow
                if (cursor_y < line_count - 1) cursor_y++;
                if (cursor_x > strlen(text[cursor_y])) cursor_x = strlen(text[cursor_y]);
                break;
            case 'C':
                if (cursor_x < MAX_COLS - 1 && cursor_x < strlen(text[cursor_y])) cursor_x++;
                break;
            case 'D':
                if (cursor_x > 0) cursor_x--;
                 break;
        }
        return;
    } else if (c == 24) { // Ctrl-X to exit
        printf("\nExiting editor...\n");
        running = 0;
    } else if (c == 19) { // Ctrl-S to save
        editor_save_file(filename);
    } else if (c == '\n' || c == '\r') {
        if (line_count < MAX_LINES - 1) {
            for (int i = line_count; i > cursor_y + 1; i--) {
                strcpy(text[i], text[i - 1]);
            }
            
            // Split and move current line to new line
            memmove(text[cursor_y + 1], &text[cursor_y][cursor_x], strlen(&text[cursor_y][cursor_x]) + 1);
            text[cursor_y][cursor_x] = '\0';

            line_count++;
            cursor_y++;
            cursor_x = 0;
        }
    } else if (c == '\b' || c == 127) { // Handle backspace
        if (cursor_x > 0) {
            cursor_x--;
            memmove(&text[cursor_y][cursor_x], &text[cursor_y][cursor_x + 1], strlen(&text[cursor_y][cursor_x + 1]) + 1);
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = strlen(text[cursor_y]);
            // Move the current line to the previous line
            strncat(text[cursor_y], text[cursor_y + 1], MAX_COLS - strlen(text[cursor_y]) - 1);
            memmove(&text[cursor_y + 1], &text[cursor_y + 2], (line_count - cursor_y - 2) * MAX_COLS);
            line_count--;
            text[line_count][0] = '\0'; // Null-terminate the last line
        }
    } else if (c >= ' ' && c <= '~') { // Printable characters
        if (cursor_x < MAX_COLS - 1) {
            for (int i = strlen(text[cursor_y]); i >= cursor_x; i--) {
                text[cursor_y][i + 1] = text[cursor_y][i]; // Shift characters to the right
            }

            text[cursor_y][cursor_x++] = c;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    int fd = syscall_open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return 1;
    }

    char buffer[256];
    int bytes_read = syscall_read(fd, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        printf("Error: Failed to read file\n");
        return 1;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the buffer
    term_get_size(&term_rows, &term_cols);
    
    char *line = strtok(buffer, "\n");
    while (line != NULL && line_count < MAX_LINES) {
        strncpy(text[line_count++], line, MAX_COLS - 1);
        line = strtok(NULL, "\n");
    }

    syscall_close(fd);

    cursor_y = line_count ? line_count - 1 : 0;
    cursor_x = strlen(text[cursor_y]);

    redraw_screen();
    while (running) {
        char c = getch();
        handle_input(c);
        redraw_screen();
    }

    // Move cursor to the end of the last line
    term_move_cursor(line_count + 1, 0);
    printf("\nEditor exited.\n");

    return 0;
}