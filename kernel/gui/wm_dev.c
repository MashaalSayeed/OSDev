/*
 * wm_dev.c — Kernel WM device driver  (/DEV/WM)
 *
 * State machine
 * =============
 *   compositor_pid == 0    →  no compositor registered yet
 *   compositor_pid != 0    →  that PID is the server
 *
 * Write path
 * ----------
 *   If caller == compositor:
 *       Expect sizeof(wm_frame_t) bytes = { target_pid, wm_response_t }.
 *       Find or ignore the connection slot for target_pid and push the
 *       response into its resp_ring.
 *
 *   If caller != compositor AND compositor_pid == 0:
 *       The first WM_REQ_CONNECT makes the caller the compositor.
 *       No inbox enqueue; return immediately.
 *
 *   If caller != compositor AND compositor_pid != 0:
 *       Expect sizeof(wm_request_t) bytes.
 *       Find-or-create a connection slot for the caller.
 *       Wrap the request as wm_frame_t{caller_pid, req} and push to inbox.
 *
 * Read path
 * ---------
 *   If caller == compositor:
 *       Pop next wm_frame_t from the global inbox. Returns 0 bytes if empty.
 *
 *   If caller != compositor:
 *       Pop next wm_response_t from caller's resp_ring. Returns 0 if empty.
 */

#include "kernel/wm_dev.h"
#include "kernel/devfs.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "kernel/process.h"
#include "kernel/locks.h"
#include "libc/string.h"

/* -----------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------- */

#define WM_MAX_CLIENTS   16
#define WM_INBOX_SLOTS   64    /* wm_frame_t entries  */
#define WM_RESP_SLOTS    32    /* wm_response_t entries per client */

typedef struct {
    uint32_t      pid;
    int           active;
    wm_response_t resp_ring[WM_RESP_SLOTS];
    uint32_t      resp_head;   /* consumer index */
    uint32_t      resp_tail;   /* producer index */
} wm_conn_t;

static uint32_t   compositor_pid = 0;
static wm_conn_t  wm_conns[WM_MAX_CLIENTS];
/* Global inbox: compositor reads wm_frame_t items from here */

/* Global inbox: compositor reads wm_frame_t items from here */
static wm_frame_t inbox[WM_INBOX_SLOTS];
static uint32_t   inbox_head = 0;
static uint32_t   inbox_tail = 0;

/* Protects compositor_pid, wm_conns and inbox/resprings */
static spinlock_t wm_lock;

/* -----------------------------------------------------------------------
 * Helper: connection management
 * --------------------------------------------------------------------- */

static int find_conn(uint32_t pid) {
    for (int i = 0; i < WM_MAX_CLIENTS; i++)
        if (wm_conns[i].active && wm_conns[i].pid == pid) return i;
    return -1;
}

static int alloc_conn(uint32_t pid) {
    for (int i = 0; i < WM_MAX_CLIENTS; i++) {
        if (!wm_conns[i].active) {
            wm_conns[i].pid       = pid;
            wm_conns[i].active    = 1;
            wm_conns[i].resp_head = 0;
            wm_conns[i].resp_tail = 0;
            return i;
        }
    }
    return -1;
}

static int find_or_alloc_conn(uint32_t pid) {
    int slot = find_conn(pid);
    if (slot < 0) slot = alloc_conn(pid);
    return slot;
}

/* -----------------------------------------------------------------------
 * Helper: inbox (wm_frame_t ring, compositor reads)
 * --------------------------------------------------------------------- */

static int inbox_push(uint32_t pid, const wm_request_t *req) {
    uint32_t next = (inbox_tail + 1) % WM_INBOX_SLOTS;
    if (next == inbox_head) return -1;   /* full */
    inbox[inbox_tail].pid = pid;
    inbox[inbox_tail].req = *req;
    inbox_tail = next;
    return 0;
}

static int inbox_pop(wm_frame_t *out) {
    if (inbox_head == inbox_tail) return 0;  /* empty */
    *out = inbox[inbox_head];
    inbox_head = (inbox_head + 1) % WM_INBOX_SLOTS;
    return 1;
}

/* -----------------------------------------------------------------------
 * Helper: per-client response ring (compositor writes, client reads)
 * --------------------------------------------------------------------- */

static int resp_push(int slot, const wm_response_t *resp) {
    wm_conn_t *c = &wm_conns[slot];
    uint32_t next = (c->resp_tail + 1) % WM_RESP_SLOTS;
    if (next == c->resp_head) return -1;  /* full */
    c->resp_ring[c->resp_tail] = *resp;
    c->resp_tail = next;
    return 0;
}

static int resp_pop(int slot, wm_response_t *out) {
    wm_conn_t *c = &wm_conns[slot];
    if (c->resp_head == c->resp_tail) return 0;  /* empty */
    *out = c->resp_ring[c->resp_head];
    c->resp_head = (c->resp_head + 1) % WM_RESP_SLOTS;
    return 1;
}

/* -----------------------------------------------------------------------
 * devfs char device callbacks
 * --------------------------------------------------------------------- */

static int wm_dev_write(vfs_device_t *dev, const char *buf, size_t count) {
    (void)dev;
    uint32_t caller_pid = get_current_process()->pid;
    uint32_t flags;

    /* ---- First-ever write: register compositor ---- */
    if (count >= sizeof(wm_request_t)) {
        spinlock_acquire_irq(&wm_lock, &flags);
        if (compositor_pid == 0) {
            const wm_request_t *req = (const wm_request_t *)buf;
            if (req->type == WM_REQ_CONNECT) {
                compositor_pid = caller_pid;
                spinlock_release_irq(&wm_lock, flags);
                kprintf(INFO, "wm_dev: compositor registered (pid=%d)\n",
                        compositor_pid);
                return (int)count;
            }
        }
        spinlock_release_irq(&wm_lock, flags);
    }

    /* ---- Compositor routing a response to a client ---- */
    spinlock_acquire_irq(&wm_lock, &flags);
    if (caller_pid == compositor_pid) {
        spinlock_release_irq(&wm_lock, flags);
        if (count < sizeof(wm_frame_t)) return -1;
        const wm_frame_t *frame = (const wm_frame_t *)buf;
        int slot = find_conn(frame->pid);
        if (slot < 0) {
            kprintf(WARNING, "wm_dev: no connection for pid=%d\n", frame->pid);
            return (int)count;   /* silently discard */
        }
        spinlock_acquire_irq(&wm_lock, &flags);
        int rc = resp_push(slot, &frame->resp);
        spinlock_release_irq(&wm_lock, flags);
        if (rc < 0)
            kprintf(WARNING, "wm_dev: resp_ring full for pid=%d\n", frame->pid);
        return (int)count;
    }
    spinlock_release_irq(&wm_lock, flags);

    /* ---- Client sending a request to the compositor ---- */
    if (count < sizeof(wm_request_t)) return -1;
    const wm_request_t *req = (const wm_request_t *)buf;

    spinlock_acquire_irq(&wm_lock, &flags);
    int slot = find_or_alloc_conn(caller_pid);
    if (slot < 0) {
        spinlock_release_irq(&wm_lock, flags);
        kprintf(ERROR, "wm_dev: connection table full\n");
        return -1;
    }

    if (req->type == WM_REQ_DISCONNECT) {
        wm_conns[slot].active = 0;
        spinlock_release_irq(&wm_lock, flags);
        return (int)count;
    }

    int rc = inbox_push(caller_pid, req);
    spinlock_release_irq(&wm_lock, flags);
    if (rc < 0)
        kprintf(WARNING, "wm_dev: inbox full, dropping request from pid=%d\n",
                caller_pid);
    return (int)count;
}

static int wm_dev_read(vfs_device_t *dev, char *buf, size_t count) {
    (void)dev;
    uint32_t caller_pid = get_current_process()->pid;
    uint32_t flags;

    /* ---- Compositor reading next client request ---- */
    spinlock_acquire_irq(&wm_lock, &flags);
    if (caller_pid == compositor_pid) {
        if (count < sizeof(wm_frame_t)) {
            spinlock_release_irq(&wm_lock, flags);
            return -1;
        }
        wm_frame_t frame;
        if (!inbox_pop(&frame)) {
            spinlock_release_irq(&wm_lock, flags);
            return 0;
        }
        memcpy(buf, &frame, sizeof(wm_frame_t));
        spinlock_release_irq(&wm_lock, flags);
        return (int)sizeof(wm_frame_t);
    }
    spinlock_release_irq(&wm_lock, flags);

    /* ---- Client reading a response from the compositor ---- */
    if (count < sizeof(wm_response_t)) return -1;
    spinlock_acquire_irq(&wm_lock, &flags);
    int slot = find_conn(caller_pid);
    if (slot < 0) {
        spinlock_release_irq(&wm_lock, flags);
        return 0;
    }
    wm_response_t resp;
    if (!resp_pop(slot, &resp)) {
        spinlock_release_irq(&wm_lock, flags);
        return 0;
    }
    memcpy(buf, &resp, sizeof(wm_response_t));
    spinlock_release_irq(&wm_lock, flags);
    return (int)sizeof(wm_response_t);
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void wm_dev_init(void) {
    memset(wm_conns, 0, sizeof(wm_conns));
    compositor_pid = 0;

    static vfs_device_t wm_device;
    memset(&wm_device, 0, sizeof(wm_device));
    strncpy(wm_device.name, "WM", DEVFS_NAME_LEN - 1);
    wm_device.type               = DEVICE_TYPE_CHAR;
    wm_device.device_data        = NULL;
    wm_device.char_dev.read_char  = wm_dev_read;
    wm_device.char_dev.write_char = wm_dev_write;

    initialize_lock(&wm_lock);

    if (devfs_register_device(&wm_device) == 0)
        kprintf(INFO, "wm_dev: registered /DEV/WM\n");
    else
        kprintf(ERROR, "wm_dev: failed to register /DEV/WM\n");
}

void wm_dev_push_input_event(wm_event_type_t type,
                              int32_t x, int32_t y,
                              uint32_t key, uint32_t btn) {
    /* Synthesise a kernel-side request (pid=0) with type WM_REQ_INPUT */
    wm_request_t req;
    memset(&req, 0, sizeof(req));
    req.type   = WM_REQ_INPUT;
    req.x      = x;
    req.y      = y;
    req.win_id = ((uint32_t)type << 16) | (btn & 0xFFFF);
    req.w      = (int32_t)key;

    uint32_t flags;
    spinlock_acquire_irq(&wm_lock, &flags);
    int rc = inbox_push(0, &req);
    spinlock_release_irq(&wm_lock, flags);
    if (rc < 0)
        kprintf(WARNING, "wm_dev: input inbox full\n");
}
