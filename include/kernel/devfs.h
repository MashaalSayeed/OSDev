#pragma once

#include "kernel/vfs.h"

int devfs_register_device(vfs_device_t *device);
vfs_device_t* devfs_get_device(const char *name);
vfs_superblock_t *devfs_mount(vfs_device_t *device);