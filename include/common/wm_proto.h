#pragma once

/*
 * wm_proto.h — Shared wire-protocol types for the user-space compositor.
 *
 * Used by both kernel (wm_dev.c) and user space (wm.h / compositor.c).
 * Keep this header free of kernel-only or user-only includes.
 */

#include <stdint.h>
#include <stddef.h>

#define WM_TITLE_MAX 64

/* -----------------------------------------------------------------------
 * Request types  (client  →  compositor, via /DEV/WM)
 * --------------------------------------------------------------------- */
typedef enum {
    WM_REQ_CONNECT    = 1,   /* Register with the compositor            */
    WM_REQ_CREATE     = 2,   /* Create a new window; resp includes shm_id */
    WM_REQ_DESTROY    = 3,   /* Destroy a window                        */
    WM_REQ_FLUSH      = 4,   /* Mark a dirty rect; compositor repaints  */
    WM_REQ_MOVE       = 5,   /* Move window to (x, y)                   */
    WM_REQ_RESIZE     = 6,   /* Resize window (new w, h; new shm issued) */
    WM_REQ_DISCONNECT = 7,   /* Clean disconnect                        */
    WM_REQ_INPUT      = 100, /* Kernel-injected input event (pid == 0)  */
} wm_req_type_t;

typedef struct {
    uint32_t type;       /* wm_req_type_t                               */
    uint32_t win_id;     /* window ID (used by DESTROY/FLUSH/MOVE)      */
    int32_t  x, y;       /* position (CREATE origin; MOVE destination)  */
    int32_t  w, h;       /* size (CREATE) or dirty rect (FLUSH)         */
    char     title[WM_TITLE_MAX];  /* window title (CREATE only)                  */
} wm_request_t;

/* -----------------------------------------------------------------------
 * Response types  (compositor  →  client, via /DEV/WM)
 * --------------------------------------------------------------------- */
typedef enum {
    WM_RESP_OK    = 0,
    WM_RESP_ERROR = 1,
    WM_RESP_EVENT = 2,   /* Carries an input event to the client        */
} wm_resp_type_t;

/* Event types embedded inside WM_RESP_EVENT */
typedef enum {
    WM_EVENT_CLOSE   = 1,
    WM_EVENT_KEY     = 2,
    WM_EVENT_MOUSE   = 3,
} wm_event_type_t;

typedef struct {
    uint32_t type;          /* wm_resp_type_t                           */
    uint32_t win_id;        /* window this response is for              */
    int32_t  shm_id;        /* SHM handle returned on CREATE            */
    int32_t  status;        /* 0 = success, negative = error            */
    /* --- event payload (WM_RESP_EVENT only) ------------------------- */
    uint32_t event_type;    /* wm_event_type_t                          */
    int32_t  event_x, event_y;       /* mouse X, Y or key scancode               */
    uint32_t event_key;     /* keyboard scancode                        */
    uint32_t event_btn;     /* mouse button mask / key modifiers        */
} wm_response_t;

/* -----------------------------------------------------------------------
 * Framing  (compositor ↔ kernel device)
 *
 *  Compositor reads  from /DEV/WM : wm_frame_t { pid=sender,   req }
 *  Compositor writes to  /DEV/WM : wm_frame_t { pid=recipient, resp }
 *  Client     writes  to /DEV/WM : wm_request_t  (kernel wraps w/ pid)
 *  Client     reads   from/DEV/WM: wm_response_t (kernel unwraps)
 * --------------------------------------------------------------------- */
typedef struct {
    uint32_t pid;
    union {
        wm_request_t  req;
        wm_response_t resp;
    };
} wm_frame_t;
