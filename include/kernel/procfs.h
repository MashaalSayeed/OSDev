#pragma once

#include "kernel/vfs.h"

vfs_superblock_t *procfs_mount(const char *device);
