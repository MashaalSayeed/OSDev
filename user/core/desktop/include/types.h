/*
 * types.h — Shared types, constants, and colour palette for the compositor.
 */
#ifndef DESKTOP_TYPES_H
#define DESKTOP_TYPES_H

#include <stdint.h>

/* ---- Layout constants ---- */
#define MAX_WINDOWS       32
#define TITLE_H           22   /* title-bar height (px)              */
#define BORDER_W           1   /* window border width (px)            */
#define TASKBAR_H         28   /* taskbar height (px)                 */
#define TASKBAR_BTN_W    110   /* width of a taskbar window button    */
#define TASKBAR_LOGO_W    64   /* width of the brand / logo area      */
#define TASKBAR_LAUNCH_W  80   /* width of the app-launch button      */

/* ---- Colour palette (0xAARRGGBB) ---- */
#define COLOR_DESKTOP         0xFF1C3B5A
#define COLOR_DESKTOP_STRIPE  0xFF193451

#define COLOR_BORDER          0xFF5A8AB0
#define COLOR_BORDER_F        0xFF89B8D8

#define COLOR_TITLEBAR        0xFF2E5C80
#define COLOR_TITLEBAR_F      0xFF1A7FBF
#define COLOR_TITLE_TEXT      0xFFDDEEFF
#define COLOR_TITLE_TEXT_UF   0xFF8AACCC

#define COLOR_BTN_CLOSE       0xFFE05050
#define COLOR_BTN_X           0xFFFFFFFF

#define COLOR_TASKBAR         0xFF111E2A
#define COLOR_TASKBAR_SEP     0xFF2A4A66
#define COLOR_TASKBAR_BTN     0xFF1E3A52
#define COLOR_TASKBAR_BTN_F   0xFF1A7FBF
#define COLOR_TASKBAR_BTN_H   0xFF2A5070
#define COLOR_TASKBAR_TEXT    0xFFCCDDEE
#define COLOR_TASKBAR_LOGO    0xFF89B8D8

#define COLOR_WIN_BG          0xFFFFFFFF

/* ---- Per-window compositor state ---- */
typedef struct {
    uint32_t  id;
    int32_t   x, y;        /* screen position of the title-bar top-left */
    int32_t   w, h;        /* client content-area size                   */
    int32_t   shm_id;
    uint32_t *pixels;      /* compositor VA into the SHM pixel buffer    */
    uint32_t  owner_pid;
    int       active;
    int       dirty;
    char      title[64];
} comp_window_t;

#endif /* DESKTOP_TYPES_H */
