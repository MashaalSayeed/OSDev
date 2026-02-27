/*
 * notepad.c — Simple graphical text editor for ZineOS.
 *
 * Uses the user-space WM client library (wm.h) to create a window and
 * renders text with the ui.h drawing primitives.  All text is stored in
 * a fixed-size line buffer; no file I/O is implemented in this version.
 *
 * Keyboard handling
 * -----------------
 *   Printable ASCII (32–126)  → insert at cursor
 *   Backspace  (\b / 0x08)    → delete character before cursor
 *   Enter      (\n or \r)     → new line
 *   ESC sequences             → ignored (arrow-key sequences from the
 *                               keyboard driver arrive as three separate
 *                               key events: ESC, '[', direction letter)
 *
 * Close button
 * ------------
 *   The compositor sends WM_EVENT_CLOSE when the red ⊗ button is clicked.
 *   Notepad exits cleanly in response.
 */

#include "user/stdio.h"
#include "user/syscall.h"
#include <stddef.h>
#include "user/wm.h"
#include "user/ui.h"
#include "libc/string.h"
#include "common/wm_proto.h"

/* -----------------------------------------------------------------------
 * Layout constants
 * --------------------------------------------------------------------- */
#define WIN_W        480
#define WIN_H        320
#define WIN_X        120
#define WIN_Y         60

#define PAD_LEFT       6
#define PAD_TOP        6
#define LINE_H        (UI_FONT_H + 2)   /* pixels per text row             */

#define MAX_LINES     38
#define MAX_LINE_LEN  58   /* chars per line (fits within WIN_W with PAD)  */

/* Derived: visible lines that fit in the window */
#define VISIBLE_LINES ((WIN_H - PAD_TOP * 2) / LINE_H)

/* -----------------------------------------------------------------------
 * Colour palette
 * --------------------------------------------------------------------- */
#define COL_BG        0xFFFFFFFF   /* white background                     */
#define COL_TEXT      0xFF1A1A2E   /* near-black text                      */
#define COL_CURSOR    0xFF1A7FBF   /* cursor bar colour                    */
#define COL_SEL_LINE  0xFFE8F4FF   /* current-line highlight               */
#define COL_GUTTER    0xFFF0F4F8   /* left gutter background               */
#define COL_GUTTER_LN 0xFF8AACCC   /* gutter line-number text              */
#define COL_STATUS_BG 0xFF1C3B5A   /* status bar background                */
#define COL_STATUS_FG 0xFFCCDDEE   /* status bar text                      */

#define GUTTER_W      28           /* pixel width of the line-number gutter */
#define STATUS_H      14           /* pixel height of the status bar        */

/* -----------------------------------------------------------------------
 * Editor state
 * --------------------------------------------------------------------- */
static char   lines[MAX_LINES][MAX_LINE_LEN + 1];
static int    num_lines   = 1;
static int    cursor_line = 0;
static int    cursor_col  = 0;
static int    scroll_top  = 0;   /* first visible line index               */

static uint32_t  win_id  = 0;
static uint32_t *pixels  = NULL;

/* -----------------------------------------------------------------------
 * Rendering
 * --------------------------------------------------------------------- */
static void render(void)
{
    /* ---- Full background ---- */
    ui_fill_rect(pixels, WIN_W, WIN_W, WIN_H, 0, 0, WIN_W, WIN_H, COL_BG);

    /* ---- Line gutter ---- */
    ui_fill_rect(pixels, WIN_W, WIN_W, WIN_H, 0, 0, GUTTER_W, WIN_H - STATUS_H, COL_GUTTER);

    /* ---- Text area ---- */
    int content_h = WIN_H - STATUS_H;
    for (int vi = 0; vi < VISIBLE_LINES; vi++) {
        int li = scroll_top + vi;
        if (li >= num_lines) break;

        int py = PAD_TOP + vi * LINE_H;
        if (py + LINE_H > content_h) break;

        /* Highlight current line */
        if (li == cursor_line)
            ui_fill_rect(pixels, WIN_W, WIN_W, WIN_H,
                         GUTTER_W, py, WIN_W - GUTTER_W, LINE_H, COL_SEL_LINE);

        /* Line number in gutter */
        char lnum[6];
        int n = li + 1;
        int d = 0;
        if (n == 0) { lnum[d++] = '0'; }
        else { char tmp[6]; int t=0; while(n>0){tmp[t++]='0'+n%10;n/=10;}
               for(int k=t-1;k>=0;k--) lnum[d++]=tmp[k]; }
        lnum[d] = '\0';
        ui_draw_string(pixels, WIN_W, WIN_W, WIN_H,
                       2, py + 1, lnum, COL_GUTTER_LN, COL_GUTTER, 0);

        /* Text */
        int tx = GUTTER_W + PAD_LEFT;
        if (li == cursor_line) {
            /* Before-cursor segment */
            char before[MAX_LINE_LEN + 1];
            int bc = cursor_col < (int)strlen(lines[li]) ? cursor_col : (int)strlen(lines[li]);
            strncpy(before, lines[li], bc);
            before[bc] = '\0';
            int cx = ui_draw_string(pixels, WIN_W, WIN_W, WIN_H,
                                    tx, py, before, COL_TEXT, COL_SEL_LINE, 0);
            /* Cursor bar */
            ui_fill_rect(pixels, WIN_W, WIN_W, WIN_H,
                         cx, py, 2, UI_FONT_H, COL_CURSOR);
            /* After-cursor segment */
            ui_draw_string(pixels, WIN_W, WIN_W, WIN_H,
                           cx + 2, py, lines[li] + bc, COL_TEXT, COL_SEL_LINE, 0);
        } else {
            ui_draw_string(pixels, WIN_W, WIN_W, WIN_H,
                           tx, py, lines[li], COL_TEXT, COL_BG, 0);
        }
    }

    /* ---- Status bar ---- */
    ui_fill_rect(pixels, WIN_W, WIN_W, WIN_H,
                 0, WIN_H - STATUS_H, WIN_W, STATUS_H, COL_STATUS_BG);
    {
        /* "Line X  Col Y  |  Notepad" */
        char status[64];
        /* Simple integer-to-string without sprintf */
        char *p = status;
        const char *lbl = "Ln ";
        while (*lbl) *p++ = *lbl++;
        int ln = cursor_line + 1;
        char tmp[8]; int t = 0;
        if (ln == 0) { tmp[t++] = '0'; }
        else { int v = ln; while(v>0){tmp[t++]='0'+v%10;v/=10;} }
        for (int k = t - 1; k >= 0; k--) *p++ = tmp[k];
        *p++ = ' '; *p++ = ' ';
        const char *lbl2 = "Col ";
        while (*lbl2) *p++ = *lbl2++;
        int col = cursor_col + 1;
        t = 0;
        if (col == 0) { tmp[t++] = '0'; }
        else { int v = col; while(v>0){tmp[t++]='0'+v%10;v/=10;} }
        for (int k = t - 1; k >= 0; k--) *p++ = tmp[k];
        *p++ = ' '; *p++ = ' '; *p++ = '|'; *p++ = ' ';
        const char *lbl3 = "Notepad";
        while (*lbl3) *p++ = *lbl3++;
        *p = '\0';

        ui_draw_string(pixels, WIN_W, WIN_W, WIN_H,
                       6, WIN_H - STATUS_H + 3,
                       status, COL_STATUS_FG, COL_STATUS_BG, 0);
    }

    wm_flush(win_id, 0, 0, WIN_W, WIN_H);
}

/* -----------------------------------------------------------------------
 * Text editing operations
 * --------------------------------------------------------------------- */
static void ensure_scroll(void)
{
    if (cursor_line < scroll_top)
        scroll_top = cursor_line;
    else if (cursor_line >= scroll_top + VISIBLE_LINES)
        scroll_top = cursor_line - VISIBLE_LINES + 1;
}

static void insert_char(char c)
{
    char *line = lines[cursor_line];
    int   len  = (int)strlen(line);
    if (len >= MAX_LINE_LEN) return;
    /* Shift right */
    for (int i = len; i > cursor_col; i--)
        line[i] = line[i - 1];
    line[cursor_col] = c;
    line[len + 1] = '\0';
    cursor_col++;
}

static void do_backspace(void)
{
    if (cursor_col > 0) {
        char *line = lines[cursor_line];
        int   len  = (int)strlen(line);
        for (int i = cursor_col - 1; i < len; i++)
            line[i] = line[i + 1];
        cursor_col--;
    } else if (cursor_line > 0) {
        /* Join with previous line */
        int prev_len = (int)strlen(lines[cursor_line - 1]);
        int curr_len = (int)strlen(lines[cursor_line]);
        if (prev_len + curr_len <= MAX_LINE_LEN) {
            strncat(lines[cursor_line - 1], lines[cursor_line],
                    MAX_LINE_LEN - prev_len);
            /* Remove current line */
            for (int i = cursor_line; i < num_lines - 1; i++)
                memcpy(lines[i], lines[i + 1], MAX_LINE_LEN + 1);
            memset(lines[num_lines - 1], 0, MAX_LINE_LEN + 1);
            num_lines--;
            cursor_line--;
            cursor_col = prev_len;
        }
    }
}

static void do_newline(void)
{
    if (num_lines >= MAX_LINES) return;

    char *line = lines[cursor_line];
    char  rest[MAX_LINE_LEN + 1];
    strncpy(rest, line + cursor_col, MAX_LINE_LEN);
    rest[MAX_LINE_LEN] = '\0';
    line[cursor_col]   = '\0';

    /* Shift lines down */
    for (int i = num_lines; i > cursor_line + 1; i--)
        memcpy(lines[i], lines[i - 1], MAX_LINE_LEN + 1);

    strncpy(lines[cursor_line + 1], rest, MAX_LINE_LEN);
    lines[cursor_line + 1][MAX_LINE_LEN] = '\0';
    cursor_line++;
    cursor_col = 0;
    num_lines++;
}

/* -----------------------------------------------------------------------
 * Event handler
 * --------------------------------------------------------------------- */
static int handle_event(const wm_event_t *evt)
{
    if (evt->type == WM_EVENT_CLOSE)
        return 0;   /* signal exit */

    if (evt->type == WM_EVENT_KEY) {
        uint32_t key = evt->key;

        if (key == 0x1B) {
            /* ESC – start of an escape sequence; discard */
        } else if (key == '\b' || key == 0x7F) {
            do_backspace();
        } else if (key == '\n' || key == '\r') {
            do_newline();
        } else if (key >= 32 && key < 127) {
            insert_char((char)key);
        }
        ensure_scroll();
        render();
    }
    return 1;   /* keep running */
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */
int main(void)
{
    if (wm_connect() != 0) {
        printf("[notepad] ERROR: wm_connect failed\n");
        syscall_exit(1);
    }

    win_id = wm_create_window(WIN_X, WIN_Y, WIN_W, WIN_H, "Notepad", &pixels);
    if (!win_id || !pixels) {
        printf("[notepad] ERROR: wm_create_window failed\n");
        wm_disconnect();
        syscall_exit(1);
    }

    /* Seed with a welcome message */
    strncpy(lines[0], "Welcome to ZineOS Notepad!", MAX_LINE_LEN);
    strncpy(lines[1], "Start typing...", MAX_LINE_LEN);
    num_lines   = 2;
    cursor_line = 2;
    cursor_col  = 0;
    /* Ensure line 2 is blank */
    lines[2][0] = '\0';
    num_lines   = 3;

    render();

    /* Event loop */
    while (1) {
        wm_event_t evt;
        if (wm_poll_event(&evt)) {
            if (!handle_event(&evt))
                break;
        }
        syscall_yield();
    }

    wm_destroy_window(win_id);
    wm_disconnect();
    syscall_exit(0);
    return 0;
}
