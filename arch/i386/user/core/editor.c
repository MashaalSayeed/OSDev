#include "user/stdio.h"
#include "user/syscall.h"
#include "libc/string.h"

#define MAX_LINES 100
#define MAX_COLS 80

char *filename;
char text[MAX_LINES][MAX_COLS];
int running = 1;
int line_count = 0;
int cursor_x = 0, cursor_y = 0;

void redraw_screen() {
    printf("\033[2J\033[H"); // ANSI escape code to clear screen and move cursor to home position
    for (int i = 0; i < line_count; i++) {
        printf("%s\n", text[i]);
    }

    printf("\033[25;1H"); // Move cursor to the bottom left corner
    printf("[Ctrl-X to exit] [Ctrl-S to save] [Line %d, Col %d]", cursor_y + 1, cursor_x + 1);
    printf("\033[%d;%dH", cursor_y + 1, cursor_x + 1);
}

void save_file(const char *filename) {
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
        // syscall_exit(0);
        running = 0;
    } else if (c == 19) { // Ctrl-S to save
        save_file(filename);
    } else if (c == '\n' || c == '\r') { // Handle Enter key
        if (line_count < MAX_LINES - 1) {
            text[line_count][cursor_x] = '\0'; // Null-terminate the current line
            line_count++;
            cursor_x = 0;
            cursor_y++;
        }
    } else if (c == '\b' || c == 127) { // Handle backspace
        if (cursor_x > 0) {
            cursor_x--;
            // Improve this :(
            text[line_count - 1][cursor_x] = '\0'; // Null-terminate the current line
        }
    } else if (c >= ' ' && c <= '~') { // Printable characters
        if (cursor_x < MAX_COLS - 1) {
            text[line_count - 1][cursor_x++] = c;
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

    printf("\033[%d;1H", line_count + 2);  // move to line after last line of text
    printf("\nEditor exited.\n");

    return 0;
}