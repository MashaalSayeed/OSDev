#pragma once

/*
 * wm_dev.h — Kernel WM device driver interface.
 *
 * The WM device is a special devfs char device mounted at /DEV/WM.
 * It acts as a mailbox between the user-space compositor and its clients:
 *
 *   Compositor reads  → wm_frame_t { sender_pid, wm_request_t  }
 *   Compositor writes → wm_frame_t { target_pid, wm_response_t }
 *   Client     writes → wm_request_t   (kernel wraps with caller's PID)
 *   Client     reads  → wm_response_t  (kernel unwraps for the caller)
 *
 * The first process to write WM_REQ_CONNECT when compositor_pid == 0
 * becomes the compositor; all subsequent openers are treated as clients.
 */

#include "kernel/vfs.h"
#include "common/wm_proto.h"

/*
 * Register the /DEV/WM device.
 * Must be called after vfs_init() mounts devfs at /DEV.
 */
void wm_dev_init(void);

/*
 * Inject a kernel-generated input event into the compositor's inbox.
 * Called by the mouse/keyboard drivers.
 * type   : WM_EVENT_KEY or WM_EVENT_MOUSE
 * x, y   : cursor position (mouse) or 0 (keyboard)
 * key    : scancode (keyboard) or 0 (mouse)
 * btn    : button mask (mouse) or modifier flags (keyboard)
 */
void wm_dev_push_input_event(wm_event_type_t type,
                              int32_t x, int32_t y,
                              uint32_t key, uint32_t btn);
