#include "user/syscall.h"

static inline int syscall(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (number), "b" (arg1), "c" (arg2), "d" (arg3)
        : "memory"
    );
    return result;
}

static inline int syscall6(int number, int arg1, int arg2, int arg3,
                            int arg4, int arg5, int arg6) {
    int result;
    __asm__ volatile (
        "push %%ebp\n"
        "mov %[a6], %%ebp\n"
        "int $0x80\n"
        "pop %%ebp\n"
        : "=a" (result)
        : "a" (number), "b" (arg1), "c" (arg2), "d" (arg3),
          "S" (arg4), "D" (arg5),
          [a6] "m" (arg6)   // "m" = memory operand, no register needed
        : "memory"
    );
    return result;
}

int syscall_write(int fd, const char *buffer, size_t size) {
    return syscall(SYSCALL_WRITE, fd, (int)buffer, size);
}

int syscall_read(int fd, char *buffer, size_t size) {
    return syscall(SYSCALL_READ, fd, (int)buffer, size);
}

int syscall_open(const char *path, int flags) {
    return syscall(SYSCALL_OPEN, (int)path, flags, 0);
}

int syscall_close(int fd) {
    return syscall(SYSCALL_CLOSE, fd, 0, 0);
}

int syscall_lseek(int fd, uint32_t offset, int whence) {
    return syscall(SYSCALL_LSEEK, fd, offset, whence);
}

void syscall_exit(int status) {
    syscall(SYSCALL_EXIT, status, 0, 0);
}

int syscall_getdents(int fd, void *dirp, size_t count) {
    return syscall(SYSCALL_GETDENTS, fd, (int)dirp, count);
}

int syscall_fork() {
    return syscall(SYSCALL_FORK, 0, 0, 0);
}

int syscall_exec(const char *path, char **args) {
    return syscall(SYSCALL_EXEC, (uintptr_t)path, (uintptr_t)args, 0);
}

int syscall_dup2(int oldfd, int newfd) {
    return syscall(SYSCALL_DUP2, oldfd, newfd, 0);
}

int syscall_waitpid(int pid, int *status, int options) {
    return syscall(SYSCALL_WAITPID, pid, (int)status, options);
}

int syscall_getpid() {
    return syscall(SYSCALL_GETPID, 0, 0, 0);
}

int syscall_fstat(int fd, stat_t *buf) {
    return syscall(SYSCALL_FSTAT, fd, (int)buf, 0);
}

int syscall_stat(const char *path, stat_t *buf) {
    return syscall(SYSCALL_STAT, (int)path, (int)buf, 0);
}

/* -----------------------------------------------------------------------
 * Shared-memory & framebuffer syscall wrappers
 * --------------------------------------------------------------------- */

int syscall_shm_create(uint32_t size) {
    return syscall(SYSCALL_SHM_CREATE, (int)size, 0, 0);
}

void *syscall_shm_map(int shm_id) {
    return (void *)syscall(SYSCALL_SHM_MAP, shm_id, 0, 0);
}

int syscall_shm_unmap(int shm_id) {
    return syscall(SYSCALL_SHM_UNMAP, shm_id, 0, 0);
}

int syscall_shm_destroy(int shm_id) {
    return syscall(SYSCALL_SHM_DESTROY, shm_id, 0, 0);
}

void *syscall_fb_map(uint32_t *out_width, uint32_t *out_height, uint32_t *out_pitch) {
    return (void *)syscall(SYSCALL_FB_MAP,
                           (int)out_width,
                           (int)out_height,
                           (int)out_pitch);
}

void syscall_yield(void) {
    syscall(SYSCALL_YIELD, 0, 0, 0);
}

void *syscall_sbrk(int incr) {
    return (void *)syscall(SYSCALL_SBRK, incr, 0, 0);
}

char *syscall_getcwd(char *buf, size_t size) {
    return (char *)syscall(SYSCALL_GETCWD, (int)buf, size, 0);
}

int syscall_chdir(const char *path) {
    return syscall(SYSCALL_CHDIR, (int)path, 0, 0);
}

int syscall_mkdir(const char *path, int mode) {
    return syscall(SYSCALL_MKDIR, (int)path, mode, 0);
}

int syscall_rmdir(const char *path) {
    return syscall(SYSCALL_RMDIR, (int)path, 0, 0);
}

int syscall_unlink(const char *path) {
    return syscall(SYSCALL_UNLINK, (int)path, 0, 0);
}

int syscall_pipe(int fds[2]) {
    return syscall(SYSCALL_PIPE, (int)fds, 0, 0);
}

uint32_t syscall_get_ticks() {
    return syscall(SYSCALL_GET_TICKS, 0, 0, 0);
}

int syscall_nanosleep(const timespec_t *req, timespec_t *rem) {
    return syscall(SYSCALL_NANOSLEEP, (int)req, (int)rem, 0);
}

void *syscall_mmap(void *addr, uint32_t length, int prot, int flags, int fd, uint32_t offset) {
    return (void *)syscall6(SYSCALL_MMAP, (int)addr, length, prot, flags, fd, offset);
}

void *syscall_munmap(void *addr, size_t length) {
    return (void *)syscall(SYSCALL_MUNMAP, (int)addr, length, 0);
}

void sleep(uint32_t seconds) {
    timespec_t req = { .tv_sec = seconds, .tv_nsec = 0 };
    syscall_nanosleep(&req, NULL);
}

void usleep(uint32_t microseconds) {
    timespec_t req = { .tv_sec = microseconds / 1000000, .tv_nsec = (microseconds % 1000000) * 1000 };
    syscall_nanosleep(&req, NULL);
}