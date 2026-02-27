/*
 * wm_comm.h — /DEV/WM protocol helpers: send responses and dispatch
 *             incoming client requests to the appropriate handler.
 */
#ifndef DESKTOP_WM_COMM_H
#define DESKTOP_WM_COMM_H

#include <stdint.h>
#include "common/wm_proto.h"

/* Write a response frame to a client process via the WM device. */
void send_response(uint32_t target_pid, wm_response_t *resp);

/* Fork + exec a user-space app; closes the compositor's wm_fd in the child. */
void launch_app(const char *path);

/* Dispatch one request frame (may be an input injection or a client request). */
void handle_request(uint32_t client_pid, const wm_request_t *req);

#endif /* DESKTOP_WM_COMM_H */
