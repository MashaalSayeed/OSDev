#pragma once

#include "common/wm_proto.h"

/* Process a WM_REQ_INPUT frame injected by the kernel input driver. */
void handle_input(const wm_request_t *req);

