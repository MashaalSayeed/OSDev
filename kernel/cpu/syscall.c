#include <kernel/syscall.h>
#include <kernel/printf.h>
#include <kernel/isr.h>
#include <drivers/keyboard.h>
#include <drivers/tty.h>
#include <kernel/vfs.h>
#include <kernel/process.h>
#include <kernel/kheap.h>
#include <kernel/gdt.h>
#include <kernel/pipe.h>
#include <user/dirent.h>
#include "libc/string.h"

extern struct tss_entry tss_entry;
extern thread_t *current_thread;
static registers_t *interrupt_frame;

// --- File operations ---

int sys_read(int fd, void *buffer, size_t size) {
    if (!buffer) return -1;
    process_t *proc = get_current_process();
    vfs_file_t* file = proc->fds[fd];
    if (!file) return -1;
    return file->file_ops->read(file, buffer, size);
}

int sys_write(int fd, const char *buffer, size_t size) {
    if (!buffer) return -1;
    process_t *proc = get_current_process();
    vfs_file_t* file = proc->fds[fd];
    if (!file) return -1;
    return file->file_ops->write(file, buffer, size);
}

int sys_open(const char *path, int flags) {
    if (!path) return -1;
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    vfs_file_t* file = vfs_open(resolved_path, flags);
    if (!file) return -1;
    return proc_alloc_fd(proc, file);
}

int sys_close(int fd) {
    process_t *proc = get_current_process();
    vfs_file_t* file = proc->fds[fd];
    if (!file) return -1;
    return file->file_ops->close(file);
}

int sys_lseek(int fd, uint32_t offset, int whence) {
    process_t *proc = get_current_process();
    vfs_file_t* file = proc->fds[fd];
    if (!file) return -1;
    return file->file_ops->seek(file, offset, whence);
}

// --- Process management ---

int sys_exit(int status) {
    kill_process(get_current_process(), status);
    return 0; // Should never return
}

int sys_waitpid(int pid, int *status, int options) {
    process_t *child = get_process(pid);
    process_t *proc = current_thread->owner;
    if (!child || child->parent != proc) return -1;
    if (child->status != TERMINATED) {
        proc->status = WAITING;
        current_thread->status = WAITING;
        kprintf(DEBUG, "[%d] Waiting for child process %d to terminate...\n", proc->pid, pid);
        schedule(interrupt_frame);
    }
    return -1;
}

int sys_fork() {
    return fork(interrupt_frame);
}

int sys_exec(const char *path, char **args) {
    return exec(path, args);
}

// --- Directory and file system ---

int sys_getdents(int fd, linux_dirent_t *dirp, int count) {
    return vfs_getdents(fd, dirp, count);
}

int sys_getpid() {
    return get_current_process()->pid;
}

int sys_dup2(int oldfd, int newfd) {
    process_t *proc = get_current_process();
    if (oldfd == newfd) return newfd;
    if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || newfd < 0 || newfd >= MAX_OPEN_FILES) return -1;
    if (proc->fds[oldfd] == NULL) return -1;
    if (proc->fds[newfd] != NULL) sys_close(newfd);
    proc->fds[newfd] = proc->fds[oldfd];
    proc->fds[newfd]->ref_count++;
    return newfd;
}

char *sys_getcwd(char *buf, size_t size) {
    process_t *proc = get_current_process();
    if (!buf) return NULL;
    strncpy(buf, proc->cwd, size);
    if (size > 0) buf[size - 1] = '\0';
    return buf;
}

int sys_chdir(const char *path) {
    if (!path) return -1;
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    strncpy(proc->cwd, resolved_path, sizeof(proc->cwd));
    return 0;
}

int sys_mkdir(const char *path, int mode) {
    if (!path) return -1;
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_create(resolved_path, VFS_MODE_DIR);
}

int sys_rmdir(const char *path) {
    if (!path) return -1;
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_unlink(resolved_path);
}

int sys_unlink(const char *path) {
    if (!path) return -1;
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_unlink(resolved_path);
}

// --- Memory management ---

void *sys_sbrk(int incr) {
    process_t *proc = get_current_process();
    return sbrk(proc, incr);
}

// --- Pipes ---

int sys_pipe(int fds[2]) {
    vfs_file_t *rf, *wf;
    process_t *proc = get_current_process();
    if (pipe_create(&rf, &wf, 4096) != 0) return -1;

    fds[0] = proc_alloc_fd(proc, rf);
    fds[1] = proc_alloc_fd(proc, wf);
    return 0;
}

// --- Custom syscalls ---

void sys_set_cursor(int x, int y) {
    terminal_set_cursor(x, y);
}

// --- Syscall table ---

syscall_t syscall_table[] = {
    [SYSCALL_READ]     = (syscall_t)sys_read,
    [SYSCALL_WRITE]    = (syscall_t)sys_write,
    [SYSCALL_OPEN]     = (syscall_t)sys_open,
    [SYSCALL_CLOSE]    = (syscall_t)sys_close,
    [SYSCALL_EXIT]     = (syscall_t)sys_exit,
    [SYSCALL_GETDENTS] = (syscall_t)sys_getdents,
    [SYSCALL_GETPID]   = (syscall_t)sys_getpid,
    [SYSCALL_FORK]     = (syscall_t)sys_fork,
    [SYSCALL_EXEC]     = (syscall_t)sys_exec,
    [SYSCALL_WAITPID]  = (syscall_t)sys_waitpid,
    [SYSCALL_GETCWD]   = (syscall_t)sys_getcwd,
    [SYSCALL_CHDIR]    = (syscall_t)sys_chdir,
    [SYSCALL_DUP2]     = (syscall_t)sys_dup2,
    [SYSCALL_MKDIR]    = (syscall_t)sys_mkdir,
    [SYSCALL_RMDIR]    = (syscall_t)sys_rmdir,
    [SYSCALL_UNLINK]   = (syscall_t)sys_unlink,
    [SYSCALL_LSEEK]    = (syscall_t)sys_lseek,
    [SYSCALL_SBRK]     = (syscall_t)sys_sbrk,
    [SYSCALL_PIPE]     = (syscall_t)sys_pipe,
    // [SYSCALL_YIELD] = (syscall_t)schedule,
};

// --- Syscall dispatcher ---

void syscall_handler(registers_t *regs) {
    interrupt_frame = regs;
    size_t num_syscalls = sizeof(syscall_table) / sizeof(syscall_table[0]);
    if (regs->eax < num_syscalls && syscall_table[regs->eax] != NULL) {
        regs->eax = syscall_table[regs->eax](regs->ebx, regs->ecx, regs->edx);
    } else {
        kprintf(WARNING, "Unknown syscall: %d\n", regs->eax);
        regs->eax = -1;
    }
}
