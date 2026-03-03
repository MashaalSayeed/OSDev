#include "libc/string.h"
#include "user/stdlib.h"
#include "user/stdio.h"
#include "user/syscall.h"
#include "user/term.h"
#include "libc/stdio.h"

// ─── Limits ──────────────────────────────────────────────────────────────────
#define MAX_LINES       500
#define MAX_COLS        256
#define TAB_SIZE        4

// ─── Editor state ─────────────────────────────────────────────────────────────
static char  *filename      = NULL;
static char   text[MAX_LINES][MAX_COLS];
static int    line_count    = 0;   // number of lines currently in buffer
static int    cursor_x      = 0;   // column  (index into text[cursor_y])
static int    cursor_y      = 0;   // logical line
static int    scroll_row    = 0;   // first visible logical line
static int    scroll_col    = 0;   // first visible column
static int    dirty         = 0;   // unsaved changes flag
static int    running       = 1;

static int    term_rows     = 24;
static int    term_cols     = 80;
static char   status_msg[MAX_COLS] = "";
static int    status_is_err = 0;   // 1 → show in error style

// ─── Terminal helpers ─────────────────────────────────────────────────────────
static void set_status(const char *msg, int is_error) {
    strncpy(status_msg, msg, sizeof(status_msg) - 1);
    status_msg[sizeof(status_msg) - 1] = '\0';
    status_is_err = is_error;
}

// Number of rows available for text (everything except the status bar)
static inline int editor_rows() { return term_rows - 1; }

// ─── Rendering ────────────────────────────────────────────────────────────────
static void redraw_screen() {
    term_clear_screen();

    int visible = editor_rows();
    for (int row = 0; row < visible; row++) {
        int lnum = scroll_row + row;
        if (lnum < line_count) {
            // Render the visible slice of this line
            char *line  = text[lnum];
            int   len   = strlen(line);
            int   start = scroll_col;
            if (start > len) start = len;

            int draw_len = len - start;
            if (draw_len > term_cols) draw_len = term_cols;

            if (draw_len > 0) {
                // Write exactly draw_len characters
                for (int c = 0; c < draw_len; c++) {
                    putchar(line[start + c]);
                }
            }
        }
        // Move to the next row (clear to end-of-line not available on all
        // terminals, so we rely on term_clear_screen having cleared everything)
        putchar('\n');
    }

    // ── Status bar ──────────────────────────────────────────────────────────
    term_move_cursor(term_rows - 1, 0);

    // Left side: key hints + dirty marker
    char left[MAX_COLS];
    snprintf(left, sizeof(left), " ^X Exit  ^S Save  %s",
             dirty ? "[modified]" : "");

    // Right side: position info
    char right[64];
    snprintf(right, sizeof(right), "%s  Ln %d/%d  Col %d ",
             filename ? filename : "[no name]",
             cursor_y + 1, line_count ? line_count : 1,
             cursor_x + 1);

    // Centre: status message
    int left_len  = strlen(left);
    int right_len = strlen(right);
    int msg_len   = strlen(status_msg);

    printf("%s", left);

    // Pad centre
    int pad_left  = (term_cols - left_len - right_len - msg_len) / 2;
    int pad_right = term_cols - left_len - right_len - msg_len - pad_left;
    for (int i = 0; i < pad_left && i + left_len + msg_len + right_len < term_cols; i++)
        putchar(' ');
    printf("%s", status_msg);
    for (int i = 0; i < pad_right && i + left_len + msg_len + right_len + pad_left < term_cols; i++)
        putchar(' ');

    printf("%s", right);

    // Place the hardware cursor
    term_move_cursor(cursor_y - scroll_row, cursor_x - scroll_col);
}

// ─── Scrolling ────────────────────────────────────────────────────────────────
static void scroll_to_cursor() {
    int rows = editor_rows();

    if (cursor_y < scroll_row)
        scroll_row = cursor_y;
    if (cursor_y >= scroll_row + rows)
        scroll_row = cursor_y - rows + 1;

    if (cursor_x < scroll_col)
        scroll_col = cursor_x;
    if (cursor_x >= scroll_col + term_cols)
        scroll_col = cursor_x - term_cols + 1;
}

// ─── File I/O ─────────────────────────────────────────────────────────────────
static void editor_load_file(const char *path) {
    int fd = syscall_open(path, O_RDONLY);
    if (fd < 0) {
        // New file — start with one empty line
        line_count = 1;
        text[0][0] = '\0';
        set_status("New file", 0);
        return;
    }

    // Use fstat to allocate exactly the right buffer
    stat_t st;
    if (syscall_fstat(fd, &st) < 0 || st.size == 0) {
        syscall_close(fd);
        line_count = 1;
        text[0][0] = '\0';
        set_status("Empty file", 0);
        return;
    }

    char *buf = malloc(st.size + 1);
    if (!buf) {
        syscall_close(fd);
        set_status("Out of memory", 1);
        return;
    }

    int n = syscall_read(fd, buf, st.size);
    syscall_close(fd);

    if (n < 0) {
        free(buf);
        set_status("Read error", 1);
        return;
    }
    buf[n] = '\0';

    // Split into lines
    line_count = 0;
    char *p = buf;
    while (*p && line_count < MAX_LINES) {
        char *end = p;
        while (*end && *end != '\n' && *end != '\r') end++;

        int len = end - p;
        if (len >= MAX_COLS) len = MAX_COLS - 1;
        memcpy(text[line_count], p, len);
        text[line_count][len] = '\0';
        line_count++;

        if (*end == '\r') end++;  // handle CRLF
        if (*end == '\n') end++;
        p = end;
    }

    if (line_count == 0) {
        line_count = 1;
        text[0][0] = '\0';
    }

    free(buf);
    dirty = 0;
    set_status("File loaded", 0);
}

static void editor_save_file(const char *path) {
    if (!path) {
        set_status("No filename", 1);
        return;
    }

    int fd = syscall_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        set_status("Cannot open file for writing", 1);
        return;
    }

    for (int i = 0; i < line_count; i++) {
        int len = strlen(text[i]);
        if (len > 0) syscall_write(fd, text[i], len);
        syscall_write(fd, "\n", 1);
    }

    syscall_close(fd);
    dirty = 0;
    set_status("Saved", 0);
}

// ─── Input handling ───────────────────────────────────────────────────────────

// Clamp cursor_x to the current line length
static void clamp_cursor_x() {
    int len = strlen(text[cursor_y]);
    if (cursor_x > len) cursor_x = len;
}

static void handle_input(char c) {
    // Clear transient status on any keystroke
    if (!status_is_err) status_msg[0] = '\0';
    status_is_err = 0;

    // ── Escape sequences (arrow keys etc.) ──────────────────────────────────
    if (c == '\033') {
        char seq1 = getch();
        if (seq1 != '[') return;   // unknown sequence
        char seq2 = getch();

        switch (seq2) {
            case 'A':  // Up
                if (cursor_y > 0) { cursor_y--; clamp_cursor_x(); }
                break;
            case 'B':  // Down
                if (cursor_y < line_count - 1) { cursor_y++; clamp_cursor_x(); }
                break;
            case 'C':  // Right
                if (cursor_x < (int)strlen(text[cursor_y]))
                    cursor_x++;
                else if (cursor_y < line_count - 1) {
                    cursor_y++;
                    cursor_x = 0;
                }
                break;
            case 'D':  // Left
                if (cursor_x > 0)
                    cursor_x--;
                else if (cursor_y > 0) {
                    cursor_y--;
                    cursor_x = strlen(text[cursor_y]);
                }
                break;
            case 'H':  // Home
                cursor_x = 0;
                break;
            case 'F':  // End
                cursor_x = strlen(text[cursor_y]);
                break;
            case '5':  // Page Up  (followed by ~)
                getch();
                cursor_y -= editor_rows();
                if (cursor_y < 0) cursor_y = 0;
                clamp_cursor_x();
                break;
            case '6':  // Page Down (followed by ~)
                getch();
                cursor_y += editor_rows();
                if (cursor_y >= line_count) cursor_y = line_count - 1;
                clamp_cursor_x();
                break;
        }
        scroll_to_cursor();
        return;
    }

    // ── Control keys ─────────────────────────────────────────────────────────
    if (c == 24) {          // Ctrl-X  exit
        if (dirty) {
            set_status("Unsaved changes! Press ^X again to quit anyway", 1);
            dirty = -1;     // arm: next ^X will actually quit
        } else if (dirty == -1) {
            running = 0;
        } else {
            running = 0;
        }
        return;
    }

    // Reset the "armed" quit if the user presses anything other than ^X
    if (dirty == -1) dirty = 1;

    if (c == 19) {          // Ctrl-S  save
        editor_save_file(filename);
        return;
    }

    if (c == 7) {           // Ctrl-G  go to line
        // Simple implementation: just show position
        char msg[64];
        snprintf(msg, sizeof(msg), "Line %d of %d", cursor_y + 1, line_count);
        set_status(msg, 0);
        return;
    }

    // ── Enter ─────────────────────────────────────────────────────────────────
    if (c == '\n' || c == '\r') {
        if (line_count >= MAX_LINES) return;

        // Shift lines down
        for (int i = line_count; i > cursor_y + 1; i--)
            memcpy(text[i], text[i - 1], MAX_COLS);

        // Split current line at cursor
        memcpy(text[cursor_y + 1], &text[cursor_y][cursor_x],
               strlen(&text[cursor_y][cursor_x]) + 1);
        text[cursor_y][cursor_x] = '\0';

        line_count++;
        cursor_y++;
        cursor_x = 0;
        dirty = 1;
        scroll_to_cursor();
        return;
    }

    // ── Backspace ─────────────────────────────────────────────────────────────
    if (c == '\b' || c == 127) {
        if (cursor_x > 0) {
            // Delete character before cursor
            cursor_x--;
            int len = strlen(text[cursor_y]);
            memmove(&text[cursor_y][cursor_x],
                    &text[cursor_y][cursor_x + 1],
                    len - cursor_x);
            dirty = 1;
        } else if (cursor_y > 0) {
            // Merge this line into the previous one
            int prev_len = strlen(text[cursor_y - 1]);
            int cur_len  = strlen(text[cursor_y]);
            if (prev_len + cur_len < MAX_COLS - 1) {
                memcpy(&text[cursor_y - 1][prev_len], text[cursor_y], cur_len + 1);
                // Shift remaining lines up
                for (int i = cursor_y; i < line_count - 1; i++)
                    memcpy(text[i], text[i + 1], MAX_COLS);
                text[line_count - 1][0] = '\0';
                line_count--;
                cursor_y--;
                cursor_x = prev_len;
                dirty = 1;
            }
        }
        scroll_to_cursor();
        return;
    }

    // ── Tab ───────────────────────────────────────────────────────────────────
    if (c == '\t') {
        int spaces = TAB_SIZE - (cursor_x % TAB_SIZE);
        int len = strlen(text[cursor_y]);
        if (len + spaces >= MAX_COLS) return;

        memmove(&text[cursor_y][cursor_x + spaces],
                &text[cursor_y][cursor_x],
                len - cursor_x + 1);
        for (int i = 0; i < spaces; i++)
            text[cursor_y][cursor_x + i] = ' ';
        cursor_x += spaces;
        dirty = 1;
        scroll_to_cursor();
        return;
    }

    // ── Printable character ───────────────────────────────────────────────────
    if (c >= ' ' && c <= '~') {
        int len = strlen(text[cursor_y]);
        if (len >= MAX_COLS - 1) return;  // line full

        // Shift tail right
        memmove(&text[cursor_y][cursor_x + 1],
                &text[cursor_y][cursor_x],
                len - cursor_x + 1);
        text[cursor_y][cursor_x] = c;
        cursor_x++;
        dirty = 1;
        scroll_to_cursor();
        return;
    }
}

// ─── Entry point ──────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    filename = argv[1];

    term_get_size(&term_rows, &term_cols);
    if (term_rows < 3 || term_cols < 10) {
        printf("Terminal too small\n");
        return 1;
    }

    editor_load_file(filename);

    // Start cursor at end of last line (convenient for appending)
    cursor_y = line_count - 1;
    cursor_x = strlen(text[cursor_y]);
    scroll_to_cursor();

    redraw_screen();

    while (running) {
        char c = getch();
        handle_input(c);
        redraw_screen();
    }

    term_clear_screen();
    term_move_cursor(0, 0);

    if (dirty > 0)
        printf("Warning: unsaved changes in %s\n", filename);
    else
        printf("Editor closed.\n");

    return 0;
}