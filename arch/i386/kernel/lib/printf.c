#include <stdbool.h>
#include "kernel/printf.h"
#include "libc/stdio.h"
#include "kernel/framebuffer.h"
#include "drivers/tty.h"


bool is_vbe_enabled = false;
void puts(const char* str) {
    if (is_vbe_enabled) {
    } else {
        terminal_writestring(str);
    }
}

void putchar(char c) {
    if (is_vbe_enabled) {

    } else {
        terminal_putchar(c);
    }
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    int ret = vsnprintf(buffer, sizeof(buffer), format, args);
    puts(buffer);
    va_end(args);
    return ret;
}

int kprintf(log_level_t level, const char *format, ...) {
    switch (level) {
        case DEBUG:
            terminal_setcolor(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            break;
        case INFO:
            terminal_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            break;
        case WARNING:
            terminal_setcolor(VGA_COLOR_BROWN, VGA_COLOR_BLACK);
            break;
        case ERROR:
            terminal_setcolor(VGA_COLOR_RED, VGA_COLOR_BLACK);
            break;
    }

    va_list args;
    va_start(args, format);
    char buffer[1024];
    int ret = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    puts(buffer);
    terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);  // Reset color
    return ret;
}

void hexdump(const void *addr, size_t len) {
    const uint8_t *p = (const uint8_t *)addr;
    for (size_t i = 0; i < len; i += 8) {
        printf("%x: ", (uint32_t)(uintptr_t)(p + i));  // Print address
        for (size_t j = 0; j < 8; j++) {
            if (i + j < len)
                printf("%x ", p[i + j]);  // Print hex bytes
            else
                printf("   ");
        }
        printf(" |");
        for (size_t j = 0; j < 8; j++) {
            if (i + j < len)
                printf("%c", (p[i + j] >= 32 && p[i + j] <= 126) ? p[i + j] : '.'); // ASCII
        }
        printf("|\n");
    }
}