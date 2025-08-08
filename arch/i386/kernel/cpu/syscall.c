#include <kernel/syscall.h>
#include <kernel/printf.h>
#include <kernel/isr.h>
#include <drivers/keyboard.h>
#include <kernel/vfs.h>
#include <user/dirent.h>
#include <kernel/process.h>
#include "libc/string.h"
#include "kernel/kheap.h"
#include "kernel/gdt.h"

extern struct tss_entry tss_entry;
extern thread_t *current_thread;
static registers_t *interrupt_frame;

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
    if (fd != file->fd) printf("fd: %d, fd?: %d, buf: %s\n", fd, file->fd, buffer); // Debug Redirected FD

    return file->file_ops->write(file, buffer, size);
}

int sys_open(const char *path, int flags) {
    if (!path) return -1;

    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_open(resolved_path, flags);
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

int sys_exit(int status) {
    kill_process(get_current_process(), status);
    return 0; // Should never return
}


int sys_waitpid(int pid, int *status, int options) {
    process_t *child, *proc;
    child = get_process(pid);
    proc = current_thread->owner;
    // printf("sys_waitpid: pid: %d, status: %x, options: %d\n", pid, status, options);
    if (!child || child->parent != proc) return -1;

    if (child->status != TERMINATED) {
        proc->status = WAITING;
        current_thread->status = WAITING;
        printf("[%d] Waiting for child process %d to terminate...\n", proc->pid, pid);
        // Save current process state and schedule another process
        schedule(interrupt_frame);
    }

    // printf("pid: %d, status: %x, options: %d\n", pid, status, options);
    // proc = get_current_process();
    // child = get_process(pid);
    // if (child && child->status == TERMINATED) {
    //     printf("Child Cleanup\n");
    //     if (status) *status = 0;
    //     cleanup_process(child);
    //     return pid;
    // }

    return -1;
}

int sys_getdents(int fd, linux_dirent_t *dirp, int count) {
    int ret = vfs_getdents(fd, dirp, count);
    // printf("tss: %x, esp0: %x, dir: %x\n", tss_entry.esp0, current_thread->kernel_stack + PROCESS_STACK_SIZE, current_thread->owner->root_page_table);
    // print_hexdump(tss_entry.esp0 - 0x128, 0x128);
    return ret;
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
    // printf("dup2: %d -> %d\n", oldfd, newfd);
    return newfd;
}

char * sys_getcwd(char *buf, size_t size) {
    process_t *proc = get_current_process();
    if (!buf) return NULL;

    strncpy(buf, proc->cwd, size);
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

int sys_fork() {
    int ret = fork(interrupt_frame);
    return ret;
}

int sys_exec(const char *path, char **args) {
    return exec(path, args);
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

void *sys_sbrk(int incr) {
    process_t *proc = get_current_process();
    return sbrk(proc, incr);
}


//////////////// MY SYSCALLS //////////////////////
#include "drivers/tty.h"
void sys_set_cursor(int x, int y) {
    set_terminal_cursor(x, y);
}


syscall_t syscall_table[] = {
    [SYSCALL_READ]     = (syscall_t)sys_read,
    [SYSCALL_WRITE]    = (syscall_t)sys_write,
    [SYSCALL_OPEN]     = (syscall_t)sys_open,
    [SYSCALL_CLOSE]    = (syscall_t)sys_close,
    [SYSCALL_EXIT]     = (syscall_t)sys_exit,
    [SYSCALL_GETDENTS] = (syscall_t)sys_getdents,
    [SYSCALL_GETPID]   = (syscall_t)sys_getpid, // Wrap in a function for consistency
    [SYSCALL_FORK]     = (syscall_t)sys_fork,
    [SYSCALL_EXEC]     = (syscall_t)sys_exec,
    [SYSCALL_WAITPID]  = (syscall_t)sys_waitpid,
    [SYSCALL_SBRK]     = (syscall_t)sys_sbrk,
    [SYSCALL_GETCWD]   = (syscall_t)sys_getcwd,
    [SYSCALL_CHDIR]    = (syscall_t)sys_chdir,
    [SYSCALL_DUP2]     = (syscall_t)sys_dup2,
    [SYSCALL_MKDIR]    = (syscall_t)sys_mkdir,
    [SYSCALL_RMDIR]    = (syscall_t)sys_rmdir,
    [SYSCALL_UNLINK]   = (syscall_t)sys_unlink,
    [SYSCALL_LSEEK]    = (syscall_t)sys_lseek,
    [SYSCALL_SBRK]     = (syscall_t)sys_sbrk,
    // [SYSCALL_YIELD]    = (syscall_t)schedule,
};


void syscall_handler(registers_t *regs) {
    interrupt_frame = regs;

    if (regs->eax < sizeof(syscall_table) / sizeof(syscall_table[0]) &&
        syscall_table[regs->eax] != NULL) {
        regs->eax = syscall_table[regs->eax](regs->ebx, regs->ecx, regs->edx);
    } else {
        printf("Unknown syscall: %d\n", regs->eax);
        regs->eax = -1;
    }
}