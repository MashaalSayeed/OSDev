#pragma once

#include <stddef.h>
#include "kernel/vfs.h"

typedef struct pipe {
    char *buffer;
    size_t size;
    size_t read_pos;
    size_t write_pos;
    int refcount;
} pipe_t;

int pipe_create(vfs_file_t **read_end, vfs_file_t **write_end, size_t size);