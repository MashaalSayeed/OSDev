#include <kernel/syscall.h>
#include <kernel/printf.h>
#include <kernel/isr.h>
#include <drivers/keyboard.h>
#include <kernel/vfs.h>
#include <user/dirent.h>
#include <kernel/process.h>
#include "libc/string.h"

int sys_write(int fd, const char *buffer, size_t size) {
    if (!buffer) return -1; // Validate pointer

    if (fd == 1) return printf("%s", buffer); // Write to stdout
    return vfs_write(fd, buffer, size); // Call proper VFS write
}

int sys_read(int fd, void *buffer, size_t size) {
    if (!buffer) return -1; // Validate pointer

    if (fd == 0) return read_keyboard_buffer(buffer, size); // Read from stdin
    return vfs_read(fd, buffer, size);
}

int sys_open(const char *path, int flags) {
    if (!path) return -1; // Validate pointer

    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_open(resolved_path, flags);
}

int sys_close(int fd) {
    return vfs_close(fd);
}

int sys_exit(int status) {
    printf("Exited with status: %d\n", status);
    kill_process(get_current_process());
    return 0; // Should never return
}

int sys_getdents(int fd, linux_dirent_t *dirp, int count) {
    int ret = vfs_getdents(fd, dirp, count);
    return ret;
}

int sys_dup2(int oldfd, int newfd) {
    printf("Dup2 not implemented\n");
    return -1;
}

char * sys_getcwd(char *buf, size_t size) {
    process_t *proc = get_current_process();
    if (!buf) return NULL; // Validate pointer

    strncpy(buf, proc->cwd, size);
    return buf;
}

int sys_chdir(const char *path) {
    if (!path) return -1; // Validate pointer

    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;

    strncpy(proc->cwd, resolved_path, sizeof(proc->cwd));
    return 0;
}

int sys_fork(registers_t *regs) {
    process_t *proc = get_current_process();
    memcpy(&proc->context, regs, sizeof(registers_t));
    int ret = fork();
    return ret;
}

int sys_mkdir(const char *path, int mode) {
    if (!path) return -1; // Validate pointer
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_create(resolved_path, VFS_MODE_DIR);
}

int sys_rmdir(const char *path) {
    if (!path) return -1; // Validate pointer
    process_t *proc = get_current_process();
    char resolved_path[256];
    if (!vfs_relative_path(proc->cwd, path, resolved_path)) return -1;
    return vfs_unlink(resolved_path);
}

int sys_unlink(const char *path) {
    if (!path) return -1; // Validate pointer

    return vfs_unlink(path);
}

void *sys_sbrk(int incr) {
    process_t *proc = get_current_process();
    return sbrk(proc, incr);
}

void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case SYSCALL_WRITE: // Syscall write
            regs->eax = sys_write(regs->ebx, (const char *)regs->ecx, regs->edx);
            break;
        case SYSCALL_READ: // Syscall read
            regs->eax = sys_read(regs->ebx, (char *)regs->ecx, regs->edx);
            break;
        case SYSCALL_OPEN: // Syscall open
            regs->eax = sys_open((const char *)regs->ebx, regs->ecx);
            break;
        case SYSCALL_CLOSE: // Syscall close
            regs->eax = sys_close(regs->ebx);
            break;
        case SYSCALL_EXIT: // Syscall exit
            sys_exit(regs->ebx);
            break;
        case SYSCALL_GETDENTS: // Syscall getdents
            regs->eax = sys_getdents(regs->ebx, (linux_dirent_t *)regs->ecx, regs->edx);
            break;
        case SYSCALL_GETPID:
            regs->eax = get_current_process()->pid;
            break;
        case SYSCALL_FORK:
            regs->eax = sys_fork(regs);
            break;
        case SYSCALL_EXEC:
            printf("Exec not implemented\n");
            break;
        case SYSCALL_WAITPID:
            printf("Waitpid not implemented\n");
            break;
        case SYSCALL_SBRK:
            regs->eax = (uint32_t)sys_sbrk(regs->ebx);
            break;
        case SYSCALL_GETCWD:
            regs->eax = (uint32_t)sys_getcwd((char *)regs->ebx, regs->ecx);
            break;
        case SYSCALL_CHDIR:
            regs->eax = sys_chdir((const char *)regs->ebx);
            break;
        case SYSCALL_DUP2:
            regs->eax = sys_dup2(regs->ebx, regs->ecx);
            break;
        case SYSCALL_PIPE:
            printf("Pipe not implemented\n");
            break;
        case SYSCALL_MKDIR:
            regs->eax = sys_mkdir((const char *)regs->ebx, regs->ecx);
            break;
        case SYSCALL_RMDIR:
            regs->eax = sys_rmdir((const char *)regs->ebx);
            break;
        case SYSCALL_UNLINK:
            regs->eax = sys_unlink((const char *)regs->ebx);
            break;
        default:
            printf("Unknown syscall: %d\n", regs->eax);
            regs->eax = -1;
            break;
    }
}