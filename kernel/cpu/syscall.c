#include <kernel/syscall.h>
#include <kernel/printf.h>
#include <kernel/isr.h>
#include <drivers/keyboard.h>
#include <kernel/vfs.h>
#include <kernel/process.h>
#include "libc/string.h"
#include "common/dirent.h"
#include "drivers/tty.h"
#include <kernel/pipe.h>
#include <kernel/shm.h>
#include <kernel/framebuffer.h>

extern struct tss_entry tss_entry;
extern thread_t *current_thread;
static registers_t *interrupt_frame;

// --- File operations ---
int sys_read(int fd, void *buffer, size_t size) {
    if (!buffer) return -1;
    // printf("sys_read: fd=%d, size=%d\n", fd, size);
    process_t *proc = get_current_process();
    vfs_file_t* file = proc->fds[fd];
    if (!file || !file->file_ops) return -1;
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
    return proc_close_fd(proc, fd);
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

int sys_getpid() {
    return get_current_process()->pid;
}

int sys_waitpid(int pid, int *status, int options) {
    process_t *child = get_process(pid);
    process_t *proc = current_thread->owner;
    if (!child || child->parent != proc) return -1;

    // wait_queue_sleep returns when any waker calls wait_queue_wake. 
    // If the child is not actually a zombie yet (spurious wake), waitpid falls through
    while (child->status != ZOMBIE) {
        kprintf(DEBUG, "[%d] Waiting for child process %d to terminate...\n", proc->pid, pid);
        
        // Yield to scheduler until child exits
        wait_queue_sleep(&child->wait_queue);

        // Re-fetch child process after it may have changed state (or may have been cleaned up already)
        child = get_process(pid);
        if (!child) return -1;
    }

    kprintf(DEBUG, "[%d] Cleaning up child process %d...\n", proc->pid, pid);
    if (status) *status = child->exit_code;
    cleanup_process(child);
    return pid;
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

int sys_pipe(int fds[2]) {
    vfs_file_t *rf, *wf;
    process_t *proc = get_current_process();
    if (pipe_create(&rf, &wf, 4096) != 0) return -1;

    fds[0] = proc_alloc_fd(proc, rf);
    fds[1] = proc_alloc_fd(proc, wf);
    return 0;
}

//////////////// MY SYSCALLS //////////////////////
void sys_set_cursor(int x, int y) {
    terminal_set_cursor(x, y);
}

/* --- Shared-memory syscalls --- */
int sys_shm_create(uint32_t size) {
    return shm_create(size);
}

void *sys_shm_map(int shm_id) {
    return shm_map(shm_id);
}

int sys_shm_unmap(int shm_id) {
    return shm_unmap(shm_id);
}

int sys_shm_destroy(int shm_id) {
    return shm_destroy(shm_id);
}

/*
 * sys_fb_map: map the physical framebuffer into the calling process's
 * address space.  Writes width, height, and pitch to the supplied
 * user-space pointers and returns the mapped virtual address.
 *
 * The framebuffer is always mapped at 0x90000000 in the process VA.
 */
#define FB_USER_VADDR 0x90000000u
void *sys_fb_map(uint32_t *out_width, uint32_t *out_height, uint32_t *out_pitch) {
    framebuffer_t *fb = get_framebuffer();
    if (!fb || !fb->addr) return NULL;

    process_t *proc = get_current_process();
    uint32_t size = fb->pitch * fb->height;
    map_memory(proc->root_page_table, FB_USER_VADDR, fb->addr, size, 0x7);

    if (out_width)  *out_width  = fb->width;
    if (out_height) *out_height = fb->height;
    if (out_pitch)  *out_pitch  = fb->pitch;
    return (void *)FB_USER_VADDR;
}

/* Yield: voluntarily give up the CPU to the scheduler */
int sys_yield(void) {
    if (interrupt_frame) schedule(interrupt_frame);
    return 0;
}

/* Read one raw input event (compositor polls this for mouse/keyboard) */
// int sys_input_read(input_event_t *evt) {
//     if (!evt) return -1;
//     return input_pop_event(evt) ? 1 : 0;
// }


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
    [SYSCALL_PIPE]     = (syscall_t)sys_pipe,
    /* --- SHM / framebuffer / misc --- */
    [SYSCALL_SHM_CREATE]  = (syscall_t)sys_shm_create,
    [SYSCALL_SHM_MAP]     = (syscall_t)sys_shm_map,
    [SYSCALL_SHM_UNMAP]   = (syscall_t)sys_shm_unmap,
    [SYSCALL_SHM_DESTROY] = (syscall_t)sys_shm_destroy,
    [SYSCALL_FB_MAP]      = (syscall_t)sys_fb_map,
    [SYSCALL_YIELD]       = (syscall_t)sys_yield,
    // [SYSCALL_INPUT_READ]  = (syscall_t)sys_input_read,
};


void syscall_handler(registers_t *regs) {
    interrupt_frame = regs;

    if (regs->eax < sizeof(syscall_table) / sizeof(syscall_table[0]) &&
        syscall_table[regs->eax] != NULL) {
        regs->eax = syscall_table[regs->eax](regs->ebx, regs->ecx, regs->edx);
    } else {
        kprintf(WARNING, "Unknown syscall: %d\n", regs->eax);
        regs->eax = -1;
    }
}