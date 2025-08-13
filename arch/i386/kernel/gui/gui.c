#include "kernel/gui.h"
#include "kernel/printf.h"
#include "kernel/framebuffer.h"
#include "kernel/window.h"
#include "drivers/mouse.h"
#include "drivers/pit.h"
#include "libc/stdio.h"
#include "kernel/process.h"
#include "kernel/font.h"

#define DIRTY_LIST_SIZE 64
#define CURSOR_HEIGHT 16
#define CURSOR_WIDTH 16

static window_t *window_list = NULL;
static window_t *window_list_tail = NULL;

static uint16_t screen_width, screen_height = 0;

static bool mouse_moved = false;
static bool mouse_dragging = false;
static int drag_offset_x = 0, drag_offset_y = 0;

static window_t *grabbed_window = NULL;
static bool need_redraw = true;

static rect_t dirty_list[DIRTY_LIST_SIZE];
static unsigned int dirty_list_count = 0;

psf_font_t *current_font = NULL;

static rect_t old_cursor = { 0, 0, CURSOR_WIDTH, CURSOR_HEIGHT };
static rect_t cur_cursor = { 0, 0, CURSOR_WIDTH, CURSOR_HEIGHT };
static const char *mouse_cursor[CURSOR_HEIGHT] = {
    "XX              ",
    "X.X             ",
    "X..X            ",
    "X...X           ",
    "X....X          ",
    "X.....X         ",
    "X......X        ",
    "X.......X       ",
    "X........X      ",
    "X......XXXXX    ",
    "X...X..X        ",
    "X..X X..X       ",
    "X.X  X..X       ",
    "XX    X..X      ",
    "      X..X      ",
    "       XX       "
};

void dirty_list_add(rect_t rect) {
    for (int i = 0; i < dirty_list_count; i++) {
        if (rects_overlap(dirty_list[i], rect)) {
            dirty_list[i] = rect_union(dirty_list[i], rect);
            return; // merged into existing rect
        }
    }
    if (dirty_list_count < DIRTY_LIST_SIZE) {
        dirty_list[dirty_list_count++] = rect;
    }
}

bool any_window_dirty() {
    window_t *current = window_list;
    while (current) {
        if (current->dirty) return true;
        current = current->next;
    }
    return false;
}

void dirty_flush() {
    for (unsigned int i = 0; i < dirty_list_count; i++) {
        rect_t rect = dirty_list[i];
        update_framebuffer_region(rect);
    }
    dirty_list_count = 0;
}

void repaint_rect(rect_t rect) {
    fill_fb_area(rect, 0x000000);
    dirty_list_add(rect);
}

window_t * find_window_at(int x, int y) {
    window_t *current = window_list;
    while (current) {
        if (x >= current->rect.x && x < current->rect.x + current->rect.w &&
            y >= current->rect.y && y < current->rect.y + current->rect.h) {
            return current; // Found the window at (x, y)
        }
        current = current->next;
    }
    return NULL; // No window found at (x, y)
}

void create_main_window() {
    window_t *main_window = create_window(100, 100, 800, 600, "Main Window");
    if (!main_window) {
        printf("Error: Failed to create main window\n");
        return;
    }

    fill_window(main_window, 0xFFFFFF);
    draw_titlebar(main_window, current_font);
    draw_window_border(main_window, 0x000000);

    window_list = main_window;
    window_list_tail = main_window;
}

static int child_x = 100, child_y = 50;
static int child_count = 0;
void create_child_window() {
    char *window_title = kmalloc(64);
    snprintf(window_title, 64, "Child Window %d", child_count++);
    window_t *child_window = create_window(child_x, child_y, 200, 200, window_title);
    if (!child_window) {
        printf("Error: Failed to create child window\n");
        return;
    }


    fill_window(child_window, 0xFFFFFF);
    draw_titlebar(child_window, current_font);
    draw_window_border(child_window, 0x000000);

    // Add the new window to the front of the list
    window_list->prev = child_window;
    child_window->next = window_list;
    window_list = child_window;

    child_x += 20; // Increment position for next child window
    child_y += 20;
}

void gui_init(framebuffer_t *fb) {
    init_framebuffer(fb);
    screen_width = fb->width;
    screen_height = fb->height;

    ps2_mouse_init();
    current_font = load_psf_font();
    if (!current_font) {
        printf("Error: Failed to load PSF font\n");
        return;
    }

    create_main_window();

    // Create a gui refresh thread
    // add_process(create_process("GUI Loop", gui_loop, PROCESS_FLAG_KERNEL));
    gui_loop();
}

void handle_mouse_event(mouse_event_t *evt) {
    if (!evt) return;

    switch (evt->type) {
        case MOUSE_EVENT_MOVE:
            mouse_moved = true;
            cur_cursor.x = evt->x;
            cur_cursor.y = evt->y;

            if (mouse_dragging && grabbed_window) {
                rect_t old_rect = grabbed_window->rect;
                grabbed_window->rect.x = evt->x - drag_offset_x;
                grabbed_window->rect.y = evt->y - drag_offset_y;
                grabbed_window->dirty = true;

                repaint_rect(rect_union(old_rect, grabbed_window->rect));
            }

            break;
        case MOUSE_EVENT_DOWN:
            if (evt->button == 0) { // Left button
                window_t *win = find_window_at(evt->x, evt->y);
                if (win) {
                    win->focused = true;
                    win->dirty = true;

                    grabbed_window = win;
                    mouse_dragging = true;
                    drag_offset_x = evt->x - win->rect.x;
                    drag_offset_y = evt->y - win->rect.y;
                }
            }
            break;

        case MOUSE_EVENT_UP:
            if (evt->button == 0) { // Left button
                if (mouse_dragging && grabbed_window) {
                    mouse_dragging = false;
                    grabbed_window = NULL;
                }
            }
            break;
    }
}

static uint32_t cursor_backup[CURSOR_WIDTH * CURSOR_HEIGHT];
static int cursor_saved_x = -1;
static int cursor_saved_y = -1;

void cursor_save_bg(unsigned int x, unsigned int y) {
    // Save the background color under the cursor
    for (int i = 0; i < CURSOR_HEIGHT; i++) {
        for (int j = 0; j < CURSOR_WIDTH; j++) {
            cursor_backup[i * CURSOR_WIDTH + j] = fb_get_pixel(x + j, y + i);
        }
    }

    cursor_saved_x = x;
    cursor_saved_y = y;
}

void cursor_restore_bg() {
    if (cursor_saved_x != -1 && cursor_saved_y != -1) {
        for (int i = 0; i < CURSOR_HEIGHT; i++) {
            for (int j = 0; j < CURSOR_WIDTH; j++) {
                fb_put_pixel(cursor_saved_x + j, cursor_saved_y + i, cursor_backup[i * CURSOR_WIDTH + j]);
            }
        }

        old_cursor.x = cursor_saved_x;
        old_cursor.y = cursor_saved_y;
        cursor_saved_x = -1;
        cursor_saved_y = -1;
    }
}

void cursor_draw() {
    // Ensure mouse coordinates are within screen bounds
    if (cur_cursor.x < 0 || cur_cursor.x >= screen_width || cur_cursor.y < 0 || cur_cursor.y >= screen_height) {
        return;
    }

    cursor_save_bg(cur_cursor.x, cur_cursor.y);
    for (int i = 0; i < CURSOR_HEIGHT; i++) {
        for (int j = 0; j < CURSOR_WIDTH; j++) {
            if (mouse_cursor[i][j] == 'X') fb_put_pixel(cur_cursor.x + j, cur_cursor.y + i, 0xFFFFFF); // Border color for the cursor
            else if (mouse_cursor[i][j] == '.') fb_put_pixel(cur_cursor.x + j, cur_cursor.y + i, 0x000000); // Background color
        }
    }
}

void gui_loop() {
    asm volatile ("sti");
    input_event_t evt;

    while (1) {
        while (input_pop_event(&evt)) {
            if (evt.type == INPUT_TYPE_MOUSE) {
                handle_mouse_event(&evt.mouse);
            }
        }

        if (need_redraw) fill_fb(0x000000); // Clear framebuffer if redraw is needed

        if (need_redraw || mouse_moved || any_window_dirty()) {
            need_redraw = true;
            cursor_restore_bg();
        }

        window_t *current = window_list_tail;
        while (current) {
            if (current->dirty) {
                draw_window_to_fb(current);
                dirty_list_add(current->rect);
                current->dirty = false;
            }
            current = current->prev;
        }

        if (need_redraw) {
            cursor_draw();
            dirty_list_add(cur_cursor);
            dirty_list_add(old_cursor);
            dirty_flush();
            need_redraw = false;
        }

        // sleep(10);
    }
}