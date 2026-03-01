#pragma once

#include <stddef.h>
#include "kernel/vfs.h"
#include "kernel/wait_queue.h"

typedef struct pipe {
    char *buffer;
    size_t size;
    size_t read_pos;
    size_t write_pos;
    size_t data_len;
    bool write_closed;

    wait_queue_t readers;
    wait_queue_t writers;
} pipe_t;

int pipe_create(vfs_file_t **read_end, vfs_file_t **write_end, size_t size);