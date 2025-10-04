#include "kernel/pipe.h"
#include "kernel/kheap.h"
#include "kernel/vfs.h"
#include "kernel/printf.h"

#define PIPE_SIZE 4096

static int pipe_read(vfs_file_t *file, void *buf, size_t count) {
    if (!file || !buf || count == 0) return -1;
    pipe_t *pipe = (pipe_t *)file->inode->fs_data;
    if (!pipe) return -1;

    size_t bytes_read = 0;
    while (bytes_read < count && pipe->read_pos != pipe->write_pos) {
        ((char *)buf)[bytes_read++] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % pipe->size;
    }

    return bytes_read;
}

static int pipe_write(vfs_file_t *file, const void *buf, size_t count) {
    if (!file || !buf || count == 0) return -1;
    pipe_t *pipe = (pipe_t *)file->inode->fs_data;
    if (!pipe) return -1;

    size_t bytes_written = 0;
    while (bytes_written < count && ((pipe->write_pos + 1) % pipe->size) != pipe->read_pos) {
        pipe->buffer[pipe->write_pos] = ((const char *)buf)[bytes_written++];
        pipe->write_pos = (pipe->write_pos + 1) % pipe->size;
    }

    return bytes_written;
}

static int pipe_close(vfs_file_t *file) {
    if (!file) return -1;

    pipe_t *pipe = (pipe_t *)file->inode->fs_data;
    if (!pipe) {
        kfree(file->inode);
        kfree(file);
        return -1;
    }

    pipe->refcount--;
    if (pipe->refcount == 0) {
        kfree(pipe->buffer);
        kfree(pipe);
        kfree(file->inode);
    }

    kfree(file);
    return 0;
}

static struct vfs_file_operations pipe_file_ops = {
    .read = pipe_read,
    .write = pipe_write,
    .close = pipe_close,
    .seek = NULL
};

int pipe_create(vfs_file_t **read_end, vfs_file_t **write_end, size_t size) {
    if (size == 0) return -1;

    pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
    if (!pipe) return -1;

    pipe->size = size;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->refcount = 2;
    pipe->buffer = (char *)kmalloc(size);
    if (!pipe->buffer) {
        kfree(pipe);
        return -1;
    }

    vfs_inode_t *inode = (vfs_inode_t *)kmalloc(sizeof(vfs_inode_t));
    if (!inode) {
        kfree(pipe);
        return -1;
    }

    // Create common inode
    inode->mode = VFS_MODE_PIPE;
    inode->size = 0;
    inode->fs_data = pipe;
    inode->inode_ops = NULL;

    *read_end = (vfs_file_t *)kmalloc(sizeof(vfs_file_t));
    (*read_end)->inode = inode;
    (*read_end)->flags = O_RDONLY;
    (*read_end)->offset = 0;
    (*read_end)->ref_count = 1;
    (*read_end)->file_ops = &pipe_file_ops;

    *write_end = (vfs_file_t *)kmalloc(sizeof(vfs_file_t));
    (*write_end)->inode = inode;
    (*write_end)->flags = O_WRONLY;
    (*write_end)->offset = 0;
    (*write_end)->ref_count = 1;
    (*write_end)->file_ops = &pipe_file_ops;

    return 0;
}