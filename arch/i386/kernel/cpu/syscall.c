#include <kernel/syscall.h>
#include <kernel/printf.h>
#include <kernel/isr.h>
#include <drivers/keyboard.h>
#include <kernel/vfs.h>
#include <user/dirent.h>
#include <kernel/process.h>

int sys_write(int fd, const char *buffer, size_t size) {
    if (!buffer) return -1; // Validate pointer

    if (fd == 1) return printf("%s", buffer); // Write to stdout
    return vfs_write(fd, buffer, size); // Call proper VFS write
}

int sys_read(int fd, char *buffer, size_t size) {
    if (!buffer) return -1; // Validate pointer

    if (fd == 0) return kgets(buffer, size); // Read from stdin
    return vfs_read(fd, buffer, size);
}

int sys_open(const char *path, int flags) {
    if (!path) return -1; // Validate pointer

    return vfs_open(path, flags);
}

int sys_close(int fd) {
    return vfs_close(fd);
}

int sys_exit(int status) {
    printf("Exited with status: %d\n", status);
    kill_current_process();
    return 0; // Should never return
}

int sys_getdents(int fd, linux_dirent_t *dirp, int count) {
    int ret = vfs_getdents(fd, dirp, count);
    return ret;
}

int sys_mkdir(const char *path, int mode) {
    if (!path) return -1; // Validate pointer

    return vfs_create(path, VFS_MODE_DIR);
}

int sys_rmdir(const char *path) {
    if (!path) return -1; // Validate pointer

    return vfs_unlink(path);
}

int sys_unlink(const char *path) {
    if (!path) return -1; // Validate pointer

    return vfs_unlink(path);
}

void *sys_sbrk(int incr) {
    process_t *proc = get_current_process();
    return NULL;
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
            printf("Fork not implemented\n");
            break;
        case SYSCALL_EXEC:
            printf("Exec not implemented\n");
            break;
        case SYSCALL_WAITPID:
            printf("Waitpid not implemented\n");
            break;
        case SYSCALL_SBRK:
            printf("Sbrk not implemented\n");
            break;
        case SYSCALL_GETCWD:
            printf("Getcwd not implemented\n");
            break;
        case SYSCALL_CHDIR:
            printf("Chdir not implemented\n");
            break;
        case SYSCALL_DUP2:
            printf("Dup2 not implemented\n");
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