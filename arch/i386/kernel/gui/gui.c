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

extern uint32_t *backbuffer;

surface_t fb_surf, bb_surf, desktop_surf;
window_t* desktop_bar;

static window_t *window_list_top = NULL;
static window_t *window_list_bottom = NULL;

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
    int merged;
    do {
        merged = 0;
        for (int i = 0; i < dirty_list_count; i++) {
            if (rects_overlap(dirty_list[i], rect)) {
                rect = rect_union(dirty_list[i], rect);
                // Remove the merged rectangle from the list
                for (int j = i; j < dirty_list_count - 1; j++) {
                    dirty_list[j] = dirty_list[j + 1];
                }
                dirty_list_count--;
                merged = 1;
                break; // Start over since the list changed
            }
        }
    } while (merged);

    if (dirty_list_count < DIRTY_LIST_SIZE) {
        dirty_list[dirty_list_count++] = rect;
    }
}

void dirty_flush() {
    for (unsigned int i = 0; i < dirty_list_count; i++) {
        rect_t rect = dirty_list[i];
        surf_blit(&fb_surf, rect.x, rect.y, &bb_surf, rect.x, rect.y, rect.w, rect.h);
    }
    dirty_list_count = 0;
}

void draw_desktop(surface_t *surf, rect_t rect) {
    int blit_w = rect.w < desktop_surf.width ? rect.w : desktop_surf.width;
    int blit_h = rect.h < desktop_surf.height ? rect.h : desktop_surf.height;
    surf_blit(surf, rect.x, rect.y, &desktop_surf, rect.x, rect.y, blit_w, blit_h);
}

void repaint_rect(rect_t rect) {
    draw_desktop(&bb_surf, rect);

    for (window_t *win=window_list_bottom; win; win=win->next) {
        if (rects_overlap(win->rect, rect)) {
            rect_t intersect = rect_intersection(win->rect, rect);
            window_composite(win, &bb_surf, intersect);
        }
    }

    if (rects_overlap(desktop_bar->rect, rect)) {
        rect_t intersect = rect_intersection(desktop_bar->rect, rect);
        window_composite(desktop_bar, &bb_surf, intersect);
    }

    if (rects_overlap(cur_cursor, rect)) {
        cursor_draw();
    }

    surf_blit(&fb_surf, rect.x, rect.y, &bb_surf, rect.x, rect.y, rect.w, rect.h);
}

window_t * find_window_at(int x, int y) {
    window_t *current = window_list_top;
    while (current) {
        if (x >= current->rect.x && x < current->rect.x + current->rect.w &&
            y >= current->rect.y && y < current->rect.y + current->rect.h) {
            return current; // Found the window at (x, y)
        }
        current = current->prev;
    }
    return NULL;
}

void print_window_list() {
    printf("Window List (top to bottom):\n");
    window_t *current = window_list_top;
    while (current) {
        printf(" - Window: %s at (%d, %d), size: %dx%d\n", current->title, current->rect.x, current->rect.y, current->rect.w, current->rect.h);
        current = current->prev;
    }
}

void compositor_add_window(window_t *win) {
    if (!win) return;

    // Add to the front of the list
    win->next = NULL;
    win->prev = window_list_top;
    if (window_list_top) window_list_top->next = win;
    else window_list_bottom = win; // First window added

    window_list_top = win;
}

void compositor_remove_window(window_t *win) {
    if (!win) return;

    if (win->prev) win->prev->next = win->next;
    else window_list_bottom = win->next; // Removing the top window

    if (win->next) win->next->prev = win->prev;
    else window_list_top = win->prev; // Removing the bottom window

    win->next = NULL;
    win->prev = NULL;
}

void compositor_focus_window(window_t *win) {
    if (!win || win->focused) return;

    // window_list_top->focused = false; // Unfocus current top window

    // Remove from current position and add to the top
    compositor_remove_window(win);
    compositor_add_window(win);

    // win->focused = true;
    dirty_list_add(win->rect);
}

static int child_id = 1;
static int child_x = 50, child_y = 50;
void _create_child_window() {
    printf("Creating child window...\n");
    char title[32];
    snprintf(title, sizeof(title), "Child Window %d", child_id++);
    window_t *child_win = create_window(child_x, child_y, 150, 150, title);
    child_x += 10; // Offset for next child window
    child_y += 10; // Offset for next child window
    if (!child_win) {
        printf("Error: Failed to create child window\n");
        return;
    }

    surf_fill(&child_win->surface, 0xFFFFFF);
    window_draw_decorations(child_win);
    compositor_add_window(child_win);
    dirty_list_add(child_win->rect);
}

void gui_init(framebuffer_t *fb) {
    init_framebuffer(fb);
    fb_surf = (surface_t){
        .width = fb->width, .height = fb->height,
        .pitch = fb->width, .pixels = (uint32_t *)fb->addr
    };
    
    bb_surf = (surface_t){
        .width = fb->width, .height = fb->height,
        .pitch = fb->width, .pixels = backbuffer
    };

    ps2_mouse_init();
    current_font = load_psf_font();
    if (!current_font) {
        printf("Error: Failed to load PSF font\n");
        return;
    }

    window_t *main_window = create_window(300, 100, 600, 500, "Main Window");
    main_window->widgets = create_button(2, 30, "Hello, World!", _create_child_window);

    window_draw_decorations(main_window);
    window_draw_widgets(main_window);
    compositor_add_window(main_window);

    window_t *child_win = create_window(10, 10, 200, 50, "Child Window");
    surf_fill(&child_win->surface, 0xFFFFFF);
    window_draw_decorations(child_win);
    compositor_add_window(child_win);

    desktop_surf.pixels = load_bitmap_file("/RES/DESKTOP.BMP", &desktop_surf.width, &desktop_surf.height);
    if (!desktop_surf.pixels) {
        printf("Error: Failed to load desktop background bitmap\n");
        // Optionally fill with a solid color as fallback
        surf_fill(&bb_surf, 0x222222);
    } else {
        desktop_surf.pitch = desktop_surf.width;
    }
    // draw_desktop(&bb_surf, (rect_t){0, 0, bb_surf.width, bb_surf.height});

    desktop_bar = create_window(0, fb->height-40, fb->width, 40, "");
    surf_fill(&desktop_bar->surface, 0x4B9CD3);

    window_composite(desktop_bar, &bb_surf, desktop_bar->rect);
    dirty_list_add((rect_t){0, 0, fb->width, fb->height}); // Initial full redraw

    // Create a gui refresh thread
    // add_process(create_process("GUI Loop", gui_loop, PROCESS_FLAG_KERNEL));
    gui_loop();
}

void handle_mouse_event(mouse_event_t *evt) {
    if (!evt) return;
    input_event_t input_evt = { .type = INPUT_TYPE_MOUSE, .mouse = *evt };

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

                dirty_list_add(old_rect);
                dirty_list_add(grabbed_window->rect);
            }

            dirty_list_add(old_cursor);
            dirty_list_add(cur_cursor);
            break;
        case MOUSE_EVENT_DOWN:
            if (evt->button == 0) { // Left button
                window_t *win = find_window_at(evt->x, evt->y);
                if (win) {
                    compositor_focus_window(win);
                    if (in_titlebar(win, evt->x, evt->y)) {
                        if (point_in_rect(evt->x - win->rect.x, evt->y - win->rect.y, close_btn_rect(win))) {
                            printf("Close button clicked on window: %s\n", win->title);
                            dirty_list_add(win->rect);
                            compositor_remove_window(win);
                            destroy_window(win);
                            return;
                        } 
                    
                        grabbed_window = win;
                        mouse_dragging = true;
                        drag_offset_x = evt->x - win->rect.x;
                        drag_offset_y = evt->y - win->rect.y;
                    } else {
                        win->event_handler(win, &input_evt);
                    }
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
            cursor_backup[i * CURSOR_WIDTH + j] = surf_get(&bb_surf, x + j, y + i);
        }
    }

    cursor_saved_x = x;
    cursor_saved_y = y;
}

void cursor_restore_bg() {
    if (cursor_saved_x != -1 && cursor_saved_y != -1) {
        for (int i = 0; i < CURSOR_HEIGHT; i++) {
            for (int j = 0; j < CURSOR_WIDTH; j++) {
                surf_put(&bb_surf, cursor_saved_x + j, cursor_saved_y + i, cursor_backup[i * CURSOR_WIDTH + j]);
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
    if (cur_cursor.x < 0 || cur_cursor.x > fb_surf.width - cur_cursor.w || cur_cursor.y < 0 || cur_cursor.y > fb_surf.height - cur_cursor.h) {
        return;
    }

    cursor_save_bg(cur_cursor.x, cur_cursor.y);
    for (int i = 0; i < CURSOR_HEIGHT; i++) {
        for (int j = 0; j < CURSOR_WIDTH; j++) {
            if (mouse_cursor[i][j] == 'X') surf_put(&bb_surf, cur_cursor.x + j, cur_cursor.y + i, 0xFFFFFFFF); // Foreground color
            else if (mouse_cursor[i][j] == '.') surf_put(&bb_surf, cur_cursor.x + j, cur_cursor.y + i, 0x00000000); // Background color
        }
    }
}

void gui_loop() {
    asm volatile ("sti");
    input_event_t evt;
    
    while (1) {
        cursor_restore_bg();
        while (input_pop_event(&evt)) {
            if (evt.type == INPUT_TYPE_MOUSE) {
                handle_mouse_event(&evt.mouse);
            }
        }
        
        for (int i = 0; i < dirty_list_count; i++) {
            rect_t rect = dirty_list[i];
            if (rect.w > 0 && rect.h > 0) {
                repaint_rect(rect);
            }
        }

        dirty_list_count = 0;
        sleep(10); // Sleep to reduce CPU usage
    }
}