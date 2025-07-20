#include "kernel/gui.h"
#include "kernel/printf.h"
#include "kernel/framebuffer.h"
#include "kernel/window.h"
#include "drivers/mouse.h"
#include "drivers/pit.h"

static window_t *window_list = NULL;
static uint16_t screen_width = 0;
static uint16_t screen_height = 0;
static mouse_state_t *mouse_state;
static const char *mouse_cursor[16] = {
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

void gui_init(framebuffer_t *fb) {
    // Initialize framebuffer
    init_framebuffer(fb);
    screen_width = fb->width;
    screen_height = fb->height;

    printf("Framebuffer initialized: %dx%d, pitch: %d, bpp: %d\n", screen_width, screen_height, fb->pitch, fb->bpp);

    ps2_mouse_init();
    psf_font_t *font = load_psf_font();
    if (!font) {
        printf("Error: Failed to load PSF font\n");
        return;
    }

    window_t *main_window = create_window(100, 100, 800, 600, "Main Window", 0xFFFFFF);
    if (!main_window) {
        printf("Error: Failed to create main window\n");
        return;
    }

    window_list = main_window;
    gui_draw();

    // Create a gui refresh thread
    // create_thread(gui_loop, "GUI Loop", 0, NULL);
}

void mouse_draw() {
    framebuffer_t *fb = get_framebuffer();

    if (!mouse_state) {
        printf("Error: Mouse state is NULL\n");
        return;
    }

    // Ensure mouse coordinates are within screen bounds
    if (mouse_state->x < 0 || mouse_state->x >= screen_width - 16 || mouse_state->y < 0 || mouse_state->y >= screen_height - 16) {
        return; // Out of bounds, do not draw
    }

    // Draw the mouse cursor at the specified position
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (mouse_cursor[i][j] == 'X') put_pixel(mouse_state->x + j, mouse_state->y + i, 0xFFFFFF); // White color for the cursor
            else if (mouse_cursor[i][j] == '.') put_pixel(mouse_state->x + j, mouse_state->y + i, 0x000000); // Background color
        }
    }
}


void gui_draw() {
    fill_screen(0x2E3440); // Set background color
    window_t *current = window_list;
    while (current) {
        draw_window(current);
        current = current->next;
    }

    mouse_draw();
    update_framebuffer();
}

void gui_loop() {
    while (1) {
        // Handle mouse input
        mouse_state = get_mouse_state();
        if (mouse_state) {
        }
        gui_draw();
        // sleep(1);
    }
}