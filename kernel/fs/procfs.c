#include "kernel/procfs.h"
#include "kernel/printf.h"
#include "kernel/kheap.h"
#include "kernel/process.h"
#include "libc/string.h"
#include "libc/stdio.h"

extern process_t *process_list;

typedef struct {
    int pid;
    int type;
} procfs_data_t;

enum {
    PROCFS_ROOT,
    PROCFS_PID_DIR,
    PROCFS_STATUS,
    PROCFS_CMDLINE,
};

static vfs_inode_t *procfs_root = NULL;

static struct vfs_inode_operations procfs_inode_ops_root;
static struct vfs_inode_operations procfs_inode_ops_pid_dir;
static struct vfs_inode_operations procfs_inode_ops_file;

static int procfs_close(vfs_inode_t *inode) {
    if (!inode || inode == procfs_root) return -1;
    if (inode->fs_data) kfree(inode->fs_data);
    kfree(inode);
    return 0;
}

static procfs_data_t *procfs_new_data(int pid, int type) {
    procfs_data_t *d = kmalloc(sizeof(procfs_data_t));
    if (!d) return NULL;
    d->pid = pid;
    d->type = type;
    return d;
}

static vfs_inode_t *procfs_alloc_inode(vfs_superblock_t *sb, procfs_data_t *data, uint32_t mode) {
    vfs_inode_t *inode = kmalloc(sizeof(vfs_inode_t));
    if (!inode) {
        if (data) kfree(data);
        return NULL;
    }
    memset(inode, 0, sizeof(vfs_inode_t));
    inode->mode = mode;
    inode->size = 0;
    inode->fs_data = data;
    inode->superblock = sb;
    return inode;
}

static int parse_pid(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return (*s == '\0') ? n : -1;
}

static int procfs_readdir_root(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry) {
    (void)dir;
    if (offset == 0) {
        strcpy(entry->name, "self");
        entry->type = VFS_MODE_DIR;
        entry->inode_number = 0;
        return 1;
    }

    uint32_t idx = 1;
    process_t *p = process_list;
    while (p) {
        if (idx == offset) {
            snprintf(entry->name, sizeof(entry->name), "%d", p->pid);
            entry->type = VFS_MODE_DIR;
            entry->inode_number = p->pid;
            return offset + 1;
        }
        idx++;
        p = p->next;
    }

    return 0;
}

static int procfs_readdir_pid_dir(vfs_inode_t *dir, uint32_t offset, vfs_dir_entry_t *entry) {
    (void)dir;
    switch (offset) {
        case 0:
            strcpy(entry->name, "status");
            entry->type = VFS_MODE_FILE;
            entry->inode_number = 1;
            return 1;
        case 1:
            strcpy(entry->name, "cmdline");
            entry->type = VFS_MODE_FILE;
            entry->inode_number = 2;
            return 2;
        default:
            return 0;
    }
}

static vfs_inode_t *procfs_lookup_root(vfs_inode_t *dir, const char *name) {
    vfs_superblock_t *sb = dir->superblock;

    if (strcmp(name, "self") == 0) {
        process_t *cur = get_current_process();
        if (!cur) return NULL;
        procfs_data_t *d = procfs_new_data((int)cur->pid, PROCFS_PID_DIR);
        if (!d) return NULL;
        vfs_inode_t *inode = procfs_alloc_inode(sb, d, VFS_MODE_DIR);
        if (!inode) return NULL;
        inode->inode_ops = &procfs_inode_ops_pid_dir;
        return inode;
    }

    int pid = parse_pid(name);
    if (pid < 0) return NULL;

    process_t *proc = get_process((size_t)pid);
    if (!proc) return NULL;

    procfs_data_t *d = procfs_new_data(pid, PROCFS_PID_DIR);
    if (!d) return NULL;
    vfs_inode_t *inode = procfs_alloc_inode(sb, d, VFS_MODE_DIR);
    if (!inode) return NULL;
    inode->inode_ops = &procfs_inode_ops_pid_dir;
    return inode;
}

static vfs_inode_t *procfs_lookup_pid_dir(vfs_inode_t *dir, const char *name) {
    procfs_data_t *parent = (procfs_data_t *)dir->fs_data;
    vfs_superblock_t *sb = dir->superblock;
    int pid = parent->pid;
    int file_type;

    if (strcmp(name, "status") == 0) {
        file_type = PROCFS_STATUS;
    } else if (strcmp(name, "cmdline") == 0) {
        file_type = PROCFS_CMDLINE;
    } else {
        return NULL;
    }

    procfs_data_t *d = procfs_new_data(pid, file_type);
    if (!d) return NULL;
    vfs_inode_t *inode = procfs_alloc_inode(sb, d, VFS_MODE_FILE);
    if (!inode) return NULL;
    inode->inode_ops = &procfs_inode_ops_file;
    return inode;
}

static uint32_t procfs_read_file(vfs_file_t *file, void *buf, size_t count) {
    procfs_data_t *data = (procfs_data_t *)file->inode->fs_data;
    if (!data) return -1;

    char tmp[512];
    int len = 0;

    process_t *proc = get_process((size_t)data->pid);
    if (!proc) return 0;

    if (data->type == PROCFS_STATUS) {
        const char *state_str;
        switch (proc->status) {
            case READY:      state_str = "READY";     break;
            case RUNNING:    state_str = "RUNNING";   break;
            case WAITING:    state_str = "WAITING";   break;
            case SLEEPING:   state_str = "SLEEPING";  break;
            case ZOMBIE:     state_str = "ZOMBIE";    break;
            case TERMINATED: state_str = "TERMINATED"; break;
            default:         state_str = "UNKNOWN";   break;
        }

        len = snprintf(tmp, sizeof(tmp),
            "Name:\t%s\n"
            "Pid:\t%d\n"
            "State:\t%s\n",
            proc->process_name,
            (int)proc->pid,
            state_str);
    } else if (data->type == PROCFS_CMDLINE) {
        len = snprintf(tmp, sizeof(tmp), "%s", proc->process_name);
    }

    if (file->offset >= (uint32_t)len) return 0;
    size_t avail = (size_t)len - file->offset;
    if (count > avail) count = avail;

    memcpy(buf, tmp + file->offset, count);
    file->offset += count;
    return count;
}

static struct vfs_inode_operations procfs_inode_ops_root = {
    .lookup  = procfs_lookup_root,
    .readdir = procfs_readdir_root,
    .close   = procfs_close,
};

static struct vfs_inode_operations procfs_inode_ops_pid_dir = {
    .lookup  = procfs_lookup_pid_dir,
    .readdir = procfs_readdir_pid_dir,
    .close   = procfs_close,
};

static struct vfs_inode_operations procfs_inode_ops_file = {
    .read  = procfs_read_file,
    .close = procfs_close,
};

vfs_superblock_t *procfs_mount(const char *device) {
    (void)device;

    vfs_superblock_t *sb = kmalloc(sizeof(vfs_superblock_t));
    if (!sb) return NULL;
    memset(sb, 0, sizeof(vfs_superblock_t));

    procfs_root = kmalloc(sizeof(vfs_inode_t));
    if (!procfs_root) { kfree(sb); return NULL; }
    memset(procfs_root, 0, sizeof(vfs_inode_t));

    procfs_root->mode = VFS_MODE_DIR;
    procfs_root->inode_ops = &procfs_inode_ops_root;
    procfs_root->superblock = sb;

    sb->root = procfs_root;
    return sb;
}
