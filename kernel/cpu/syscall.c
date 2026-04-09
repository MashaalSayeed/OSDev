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
#include "drivers/pit.h"
#include "kernel/kheap.h"
#include "kernel/gdt.h"
#include "kernel/signals.h"

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
    int fd = proc_alloc_fd(proc, file);
    if (fd == -1) {
        vfs_close(file);
        kprintf(DEBUG, "sys_open: No available file descriptors for process %d\n", proc->pid);
        return -1;
    }
    return fd;
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

int sys_getppid() {
    process_t *parent = get_current_process()->parent;
    return parent ? parent->pid : -1;
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

int sys_exec(const char *path, char **argv) {
    process_t *proc = get_current_process();
    kprintf(DEBUG, "sys_exec: called by pid=%d\n", get_current_process()->pid);
    char k_path[256];
    if (strncpy_from_user(proc->root_page_table, k_path, (uint32_t)path, sizeof(k_path)) < 0) {
        kprintf(ERROR, "sys_exec: failed to read path\n");
        return -1;
    }

    char **k_argv = NULL;
    if (argv) {
        // Count argc
        int argc = 0;
        while (1) {
            uint32_t uptr = 0;
            copy_from_user(proc->root_page_table, &uptr,
                           (uint32_t)(argv + argc), sizeof(uint32_t));
            if (!uptr) break;
            argc++;
        }

        kprintf(DEBUG, "sys_exec: argc=%d\n", argc);
        k_argv = kmalloc((argc + 1) * sizeof(char *));
        for (int i = 0; i < argc; i++) {
            uint32_t uptr = 0;
            copy_from_user(proc->root_page_table, &uptr,
                           (uint32_t)(argv + i), sizeof(uint32_t));
            char *s = kmalloc(256);
            strncpy_from_user(proc->root_page_table, s, uptr, 256);
            k_argv[i] = s;
        }
        k_argv[argc] = NULL;
    }

    int ret = exec(k_path, k_argv);
    if (k_argv) {
        for (int i = 0; k_argv[i]; i++) kfree(k_argv[i]);
        kfree(k_argv);
    }

    return ret;
}

int sys_set_thread_area(user_desc_t *desc) {
    if (!desc) return -1;

    // musl passes entry_number = -1 to request allocation
    if (desc->entry_number == -1) {
        desc->entry_number = 6; // We only have one TLS segment, so just use that
    }

    gdt_set_tls(desc->base_addr, desc->limit);
    uint16_t selector = (GDT_TLS << 3) | 3;
    asm volatile("mov %0, %%gs" :: "r"(selector));
    current_thread->gs = selector;
    return 0;
}

int sys_set_tid_address(int *tidptr) {
    // current_thread->tidptr = tidptr;
    // return 0;
    return current_thread->tid; // Return TID as syscall result for testing purposes
}

int sys_mprotect(void *addr, size_t len, int prot) {
    return 0; // TODO: implement
}

// --- Directory and file system ---
int sys_getdents(int fd, linux_dirent_t *dirp, int count) {
    return vfs_getdents(fd, dirp, count);
}

int sys_dup(int fd) {
    // create copy of fd with lowest available fd number
    process_t *proc = get_current_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !proc->fds[fd]) return -1;

    vfs_file_t *file = proc->fds[fd];
    int newfd = proc_alloc_fd(proc, file);
    if (newfd == -1) return -1;
    file->ref_count++;
    return newfd;
}

int sys_dup2(int oldfd, int newfd) {
    process_t *proc = get_current_process();
    if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || newfd < 0 || newfd >= MAX_OPEN_FILES) return -1;
    if (proc->fds[oldfd] == NULL) return -1;

    if (oldfd == newfd) return newfd;

    if (proc->fds[newfd] != NULL) sys_close(newfd);

    proc->fds[newfd] = proc->fds[oldfd];
    proc->fds[newfd]->ref_count++;
    return newfd;
}


int sys_fcntl(int fd, int cmd, int arg) {
    process_t *proc = get_current_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !proc->fds[fd]) return -1;

    switch (cmd) {
        case F_DUPFD: {
            // Like dup but finds lowest fd >= arg
            for (int new_fd = arg; new_fd < MAX_OPEN_FILES; new_fd++) {
                if (!proc->fds[new_fd]) {
                    proc->fds[new_fd] = proc->fds[fd];
                    proc->fds[new_fd]->ref_count++;
                    return new_fd;
                }
            }
            return -1;
        }
        case F_GETFD:
            // Return fd flags (just FD_CLOEXEC for now, stub as 0)
            return 0;
        case F_SETFD:
            // Set fd flags — stub, accept but ignore FD_CLOEXEC for now
            return 0;
        case F_GETFL:
            // Return file status flags
            return proc->fds[fd]->flags;
        case F_SETFL:
            // Set file status flags
            proc->fds[fd]->flags = arg;
            return 0;
        default:
            kprintf(WARNING, "fcntl: unsupported cmd %d\n", cmd);
            return -1;
    }
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

int sys_rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) return -1;
    process_t *proc = get_current_process();

    char resolved_old[256], resolved_new[256];
    if (!vfs_relative_path(proc->cwd, oldpath, resolved_old)) return -1;
    if (!vfs_relative_path(proc->cwd, newpath, resolved_new)) return -1;

    return vfs_rename(resolved_old, resolved_new);
}

// --- Memory management ---
void *sys_brk(void *addr) {
    process_t *proc = get_current_process();
    if (!addr || addr < proc->heap_start) return proc->brk;

    void *heap_limit = proc->heap_start + PROCESS_MAX_HEAP_SIZE;
    if (addr > heap_limit) return proc->brk;

    void *new_brk = (void *)PAGE_ALIGN_UP((uintptr_t)addr);
    void *old_brk = proc->brk;

    if (new_brk > old_brk) {
        for (void *p = old_brk; p < new_brk; p += PAGE_SIZE) {
            page_table_entry_t *page = get_page((uintptr_t)p, 1, proc->root_page_table);
            if (!page || !alloc_page(page, 0x7)) {
                // FIX: partial allocation — roll back everything we mapped
                // so far to keep the heap in a consistent state
                for (void *rb = old_brk; rb < p; rb += PAGE_SIZE) {
                    page_table_entry_t *rp = get_page((uintptr_t)rb, 0, proc->root_page_table);
                    if (rp && rp->present) free_page(rp);
                }
                return old_brk;  // signal failure by returning unchanged brk
            }
        }
    } else if (new_brk < old_brk) {
        for (void *p = new_brk; p < old_brk; p += PAGE_SIZE) {
            page_table_entry_t *page = get_page((uintptr_t)p, 0, proc->root_page_table);
            if (page && page->present) free_page(page);
        }
    }

    proc->brk = new_brk;
    return proc->brk;
}

// sbrk is just a thin wrapper over brk — no independent logic
void *sys_sbrk(int incr) {
    process_t *proc = get_current_process();
    if (incr == 0) return proc->brk;

    void *old_brk = proc->brk;
    void *result = sys_brk(proc->brk + incr);

    // sbrk returns old brk on success, -1 on failure
    if (result == old_brk && incr != 0) return (void *)-1;
    return old_brk;
}

int sys_pipe(int fds[2]) {
    vfs_file_t *rf, *wf;
    process_t *proc = get_current_process();
    if (pipe_create(&rf, &wf, 4096) != 0) return -1;

    fds[0] = proc_alloc_fd(proc, rf);
    fds[1] = proc_alloc_fd(proc, wf);
    return 0;
}

int sys_stat(const char *path, stat_t *st) {
    vfs_file_t *file = vfs_open(path, VFS_FLAG_READ);
    if (!file) return -1;

    st->size = file->inode->size;
    st->mode = file->inode->mode;

    vfs_close(file);
    return 0;
}

int sys_fstat(int fd, stat_t *st) {
    process_t *proc = get_current_process();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !proc->fds[fd]) return -1;

    vfs_file_t *file = proc->fds[fd];
    st->size = file->inode->size;
    st->mode = file->inode->mode;
    return 0;
}

int sys_get_ticks() {
    return pit_get_ticks() * 10; // Convert ticks to milliseconds (assuming 100 Hz)
}

int sys_nanosleep(const timespec_t *req, timespec_t *rem) {
    if (!req) return -1;

    // Convert the request to milliseconds then to ticks
    uint32_t ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
    uint32_t ticks_needed = (ms + 9) / 10;  // ceil, 10ms per tick at 100Hz

    if (ticks_needed == 0) return 0;  // sub-10ms sleep, just return

    current_thread->wakeup_tick = pit_get_ticks() + ticks_needed;
    current_thread->status = SLEEPING;

    // Yield — pick_next_thread will skip us until wakeup_tick is reached
    schedule(NULL);

    // If we get here, we were woken up (either by tick or a signal later)
    if (rem) {
        rem->tv_sec  = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}

// Matches the Linux i386 mmap2 argument struct
void *sys_mmap(void *addr, uint32_t length, int prot, int flags, int fd, uint32_t offset) {
    process_t *proc = get_current_process();

    // We only support anonymous private mappings for now
    if (!(flags & MAP_ANONYMOUS)) {
        // File-backed mmap not implemented yet
        return (void *)-1;
    }

    if (length == 0) return (void *)-1;

    // Round up to page boundary
    uint32_t size = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // If addr is NULL, allocate from our mmap bump region
    // If addr is hinted, try to honor it but fall back to bump
    uint32_t virt;
    if (addr == NULL) {
        virt = (uint32_t)proc->mmap_base;
        proc->mmap_base += size;
    } else {
        virt = (uint32_t)addr;
    }

    // Map pages into the process's page directory
    map_memory(proc->root_page_table, virt, (uint32_t)-1, size, 0x7);

    // Zero the region — anonymous mappings must be zeroed per POSIX
    memset((void *)virt, 0, size);

    return (void *)virt;
}

void *sys_munmap(void *addr, uint32_t length) {
    process_t *proc = get_current_process();

    if (!addr || length == 0) return (void *)-1;

    uint32_t size = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t virt = (uint32_t)addr;

    // Free each page in the range
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        page_table_entry_t *page = get_page(virt + offset, 0, proc->root_page_table);
        if (page && page->present) {
            free_page(page);
        }
    }

    return (void *)0;  // success
}

int sys_ioctl(int fd, int request, void *arg) {
    // // For now just support TTY ioctls for demonstration
    // process_t *proc = get_current_process();
    // if (fd < 0 || fd >= MAX_OPEN_FILES || !proc->fds[fd]) return -1;

    // vfs_file_t *file = proc->fds[fd];
    // if (!file->file_ops || !file->file_ops->ioctl) return -1;

    // return file->file_ops->ioctl(file, request, arg);
    return -1; // ioctl not implemented yet
}

int sys_writev(int fd, const iovec_t *iov, int iovcnt) {
    // For demonstration, just implement writev for TTYs
    // process_t *proc = get_current_process();
    // if (fd < 0 || fd >= MAX_OPEN_FILES || !proc->fds[fd]) return -1;

    // vfs_file_t *file = proc->fds[fd];
    // if (!file->file_ops || !file->file_ops->write) return -1;

    // int total_written = 0;
    // for (int i = 0; i < iovcnt; i++) {
    //     total_written += file->file_ops->write(file, iov[i].iov_base, iov[i].iov_len);
    // }
    // return total_written;
    int total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (!iov[i].iov_base || iov[i].iov_len == 0) continue;
        int n = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) return n;
        total += n;
    }
    return total;
}

// Signal handling syscalls (stub implementations for now)
int sys_kill(int pid, int sig) {
    if (sig < 0 || sig >= NSIG) return -EINVAL;
    process_t *target = get_process(pid);
    if (!target) return -1;

    if (target->signal_handlers[sig].handler == SIG_IGN) return 0; // Ignored signal, consider it "handled"

    target->pending_signals |= (1 << sig);
    if (target->main_thread->status == SLEEPING || target->main_thread->status == WAITING) {
        target->main_thread->status = READY;
    }
    return 0;
}

sighandler_t sys_signal(int sig, sighandler_t handler) {
    if (sig < 0 || sig >= NSIG) return (sighandler_t)-1;
    process_t *proc = get_current_process();
    sighandler_t old_handler = proc->signal_handlers[sig].handler;
    proc->signal_handlers[sig].handler = handler;
    return old_handler;
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

static int dispatch(uint32_t num, registers_t *regs) {
    switch (num) {
        case SYSCALL_READ:     return sys_read(regs->ebx, (void *)regs->ecx, regs->edx);
        case SYSCALL_WRITE:    return sys_write(regs->ebx, (const char *)regs->ecx, regs->edx);
        case SYSCALL_OPEN:     return sys_open((const char *)regs->ebx, regs->ecx);
        case SYSCALL_CLOSE:    return sys_close(regs->ebx);
        case SYSCALL_EXIT:     return sys_exit(regs->ebx);
        case SYSCALL_GETDENTS: return sys_getdents(regs->ebx, (linux_dirent_t *)regs->ecx, regs->edx);
        case SYSCALL_GETPID:   return sys_getpid();
        case SYSCALL_FORK:     return sys_fork();
        case SYSCALL_EXEC:     return sys_exec((const char *)regs->ebx, (char **)regs->ecx);
        case SYSCALL_WAITPID:  return sys_waitpid(regs->ebx, (int *)regs->ecx, regs->edx);
        case SYSCALL_BRK:      return (int)sys_brk((void *)regs->ebx);
        case SYSCALL_SBRK:     return (int)sys_sbrk(regs->ebx);
        case SYSCALL_GETCWD:   return (int)sys_getcwd((char *)regs->ebx, regs->ecx);
        case SYSCALL_CHDIR:    return sys_chdir((const char *)regs->ebx);
        case SYSCALL_DUP:      return sys_dup(regs->ebx);
        case SYSCALL_DUP2:     return sys_dup2(regs->ebx, regs->ecx);
        case SYSCALL_FCNTL:    return sys_fcntl(regs->ebx, regs->ecx, regs->edx);
        case SYSCALL_MKDIR:    return sys_mkdir((const char *)regs->ebx, regs->ecx);
        case SYSCALL_RMDIR:    return sys_rmdir((const char *)regs->ebx);
        case SYSCALL_UNLINK:   return sys_unlink((const char *)regs->ebx);
        case SYSCALL_LSEEK:    return sys_lseek(regs->ebx, regs->ecx, regs->edx);
        case SYSCALL_PIPE:     return sys_pipe((int *)regs->ebx);
        case SYSCALL_FSTAT:    return sys_fstat((int)regs->ebx, (stat_t *)regs->ecx);
        case SYSCALL_STAT:     return sys_stat((const char *)regs->ebx, (stat_t *)regs->ecx);
        case SYSCALL_GET_TICKS: return sys_get_ticks();
        case SYSCALL_NANOSLEEP: return sys_nanosleep((const timespec_t *)regs->ebx, (timespec_t *)regs->ecx);
        case SYSCALL_SHM_CREATE:  return sys_shm_create(regs->ebx);
        case SYSCALL_SHM_MAP:     return (int)sys_shm_map(regs->ebx);
        case SYSCALL_SHM_UNMAP:   return sys_shm_unmap(regs->ebx);
        case SYSCALL_SHM_DESTROY: return sys_shm_destroy(regs->ebx);
        case SYSCALL_FB_MAP:      return (int)sys_fb_map((uint32_t *)regs->ebx, (uint32_t *)regs->ecx, (uint32_t *)regs->edx);
        case SYSCALL_YIELD:       return sys_yield();
        case SYSCALL_MMAP2:        return (int)sys_mmap((void *)regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi, regs->ebp);
        case SYSCALL_MUNMAP:      return (int)sys_munmap((void *)regs->ebx, regs->ecx);
        case SYSCALL_MPROTECT:    return sys_mprotect((void *)regs->ebx, regs->ecx, regs->edx);
        case SYSCALL_EXIT_GROUP:    return sys_exit(regs->ebx);
        case SYSCALL_SET_THREAD_AREA: return sys_set_thread_area((user_desc_t *)regs->ebx);
        case SYSCALL_SET_TID_ADDRESS: return sys_set_tid_address((int *)regs->ebx);
        case SYSCALL_IOCTL:       return sys_ioctl(regs->ebx, regs->ecx, (void *)regs->edx);
        case SYSCALL_WRITEV:      return sys_writev(regs->ebx, (const iovec_t *)regs->ecx, regs->edx);
        case SYSCALL_SIGNAL:      return (int)sys_signal((int)regs->ebx, (sighandler_t)regs->ecx);
        case SYSCALL_KILL:        return sys_kill((int)regs->ebx, (int)regs->ecx);
        case SYSCALL_GETPPID:       return sys_getppid();
        case SYSCALL_RENAME:         return sys_rename((const char *)regs->ebx, (const char *)regs->ecx);
        // case SYSCALL_INPUT_READ:  return sys_input_read((input_event_t *)regs->ebx);
        default:
            kprintf(WARNING, "Unknown syscall: %d\n", num);
            return -1;
    }
}


void syscall_handler(registers_t *regs) {
    interrupt_frame = regs;
    if (regs->eax == SYSCALL_SIGRETURN) {
        // Handle sigreturn directly in the handler since it needs to
        // restore the full context from the user stack frame
        sys_sigreturn(regs);
        interrupt_frame = NULL;
        return;
    }

    regs->eax = dispatch(regs->eax, regs);
    interrupt_frame = NULL;
}