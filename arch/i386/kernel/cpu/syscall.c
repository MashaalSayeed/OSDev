#include <kernel/syscall.h>
#include <kernel/printf.h>
#include <kernel/isr.h>
#include <drivers/keyboard.h>
#include <kernel/vfs.h>
#include <user/dirent.h>

int sys_getdents(int fd, linux_dirent_t *dirp, int count) {
    int ret = vfs_readdir(fd, dirp, count);
    if (ret <= 0) return ret;
}

void syscall_handler(registers_t *regs) {
    int fd, size;
    char *buffer;

    switch (regs->eax) {
        case 0:
            printf((const char *)regs->ebx);
            break;
        case SYSCALL_WRITE: // Syscall write
            fd = regs->ebx;
            buffer = (const char *)regs->ecx;
            size = regs->edx;
            regs->eax = printf("%s", buffer);
            break;
        case SYSCALL_READ: // Syscall read
            fd = regs->ebx;
            buffer = (const char *)regs->ecx;
            size = regs->edx;
            // printf("Reading from file descriptor: %d\n", fd);

            if (fd == 0) {
                regs->eax = kgets(buffer, size);
            } else {
                regs->eax = vfs_read(fd, buffer, size);
            }
            break;
        case SYSCALL_OPEN: // Syscall open
            regs->eax = vfs_open((const char *)regs->ebx, regs->ecx);
            break;
        case SYSCALL_CLOSE: // Syscall close
            regs->eax = vfs_close(regs->ebx);
            break;
        case SYSCALL_EXIT: // Syscall exit
            printf("Exited with status: %d\n", regs->ebx);
            kill_current_process();
            break;
        case SYSCALL_GETDENTS: // Syscall getdents
            regs->eax = vfs_getdents(regs->ebx, (char *)regs->ecx, regs->edx);
            break;
        default:
            printf("Unknown syscall: %d\n", regs->eax);
    }
}