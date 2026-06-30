#include "user/syscall.h"
#include "user/stdio.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "common/dirent.h"
#include "test_framework.h"
#include "test_utils.h"

static int str_contains(const char *haystack, const char *needle) {
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*n && *h == *n) { h++; n++; }
        if (!*n) return 1;
    }
    return 0;
}

void test_proc_self(void) {
    printf("\n[proc self]\n");

    char buf[32];
    int n = syscall_readlink("/PROC/self", buf, sizeof(buf));
    CHECK("readlink /PROC/self succeeds", n > 0, "readlink failed");
    if (n <= 0) return;

    int len = n < (int)sizeof(buf) - 1 ? n : (int)sizeof(buf) - 1;
    buf[len] = '\0';

    int is_digits = 1;
    for (int i = 0; buf[i]; i++) {
        if (buf[i] < '0' || buf[i] > '9') { is_digits = 0; break; }
    }
    CHECK("readlink returns decimal PID", is_digits, "not a number");

    int pid_from_link = 0;
    for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
        pid_from_link = pid_from_link * 10 + (buf[i] - '0');

    int my_pid = syscall_getpid();
    CHECK("PID matches getpid()", pid_from_link == my_pid, "pid mismatch");

    int no_newline = 1;
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\n') { no_newline = 0; break; }
    }
    CHECK("no newline in readlink result", no_newline, "has newline");
}

void test_proc_self_exe(void) {
    printf("\n[proc self exe]\n");

    char link_buf[64];
    int n = syscall_readlink("/proc/self/exe", link_buf, sizeof(link_buf));
    CHECK("readlink /proc/self/exe succeeds", n > 0, "readlink failed");
    if (n <= 0) return;

    int copy_len = n < (int)sizeof(link_buf) - 1 ? n : (int)sizeof(link_buf) - 1;
    link_buf[copy_len] = '\0';
    CHECK("exe path is non-empty", copy_len > 0, "empty");
}

void test_proc_pid_status(void) {
    printf("\n[proc pid status]\n");

    int my_pid = syscall_getpid();
    char path[64];
    snprintf(path, sizeof(path), "/PROC/%d/status", my_pid);

    stat_t st;
    int ret = syscall_stat(path, &st);
    CHECK("stat /PROC/pid/status succeeds", ret == 0, "stat failed");
    if (ret != 0) return;

    CHECK("S_ISREG", S_ISREG(st.mode), "not a regular file");

    int fd = syscall_open(path, O_RDONLY);
    CHECK("open /PROC/pid/status succeeds", fd >= 0, "open failed");
    if (fd < 0) return;

    char buf[256];
    int n = syscall_read(fd, buf, sizeof(buf) - 1);
    CHECK("read returns data", n > 0, "read returned 0");
    syscall_close(fd);

    if (n > 0) {
        buf[n] = '\0';

        CHECK("status contains Name:", str_contains(buf, "Name:\t"), "missing Name:");
        CHECK("status contains Pid:", str_contains(buf, "Pid:\t"), "missing Pid:");
        CHECK("status contains State:", str_contains(buf, "State:\t"), "missing State:");

        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d", my_pid);
        CHECK("status pid matches", str_contains(buf, pid_str), "pid mismatch");
    }

    fd = syscall_open(path, O_WRONLY);
    CHECK("open status O_WRONLY fails", fd < 0, "should have failed");
    if (fd >= 0) syscall_close(fd);
}

void test_proc_directory_semantics(void) {
    printf("\n[proc directory semantics]\n");

    int fd = syscall_open("/PROC", O_RDONLY);
    CHECK("open /PROC succeeds", fd >= 0, "open failed");
    if (fd < 0) return;

    char buf[64];
    int n = syscall_read(fd, buf, sizeof(buf));
    CHECK("read directory fd fails", n < 0, "should have failed");

    char dent_buf[1024];
    n = syscall_getdents(fd, dent_buf, sizeof(dent_buf));
    CHECK("getdents returns data", n > 0, "getdents failed");

    int found_self = 0;
    int found_my_pid = 0;
    int my_pid = syscall_getpid();
    char my_pid_str[16];
    snprintf(my_pid_str, sizeof(my_pid_str), "%d", my_pid);

    int off = 0;
    while (off < n) {
        linux_dirent_t *d = (linux_dirent_t *)(dent_buf + off);
        if (d->d_reclen == 0) break;
        if (strcmp(d->d_name, "self") == 0) found_self = 1;
        if (strcmp(d->d_name, my_pid_str) == 0) found_my_pid = 1;
        off += d->d_reclen;
    }
    CHECK("getdents contains 'self'", found_self, "self not found");
    CHECK("getdents contains current PID", found_my_pid, "PID dir not found");

    syscall_close(fd);

    stat_t st;
    int ret = syscall_stat("/PROC", &st);
    CHECK("stat /PROC succeeds", ret == 0, "stat failed");
    if (ret == 0) {
        CHECK("S_ISDIR", S_ISDIR(st.mode), "not a directory");
    }
}

void test_proc_invalid_paths(void) {
    printf("\n[proc invalid paths]\n");

    int fd;
    fd = syscall_open("/PROC/doesnotexist", O_RDONLY);
    CHECK("/PROC/doesnotexist fails", fd < 0, "should have failed");
    if (fd >= 0) syscall_close(fd);

    fd = syscall_open("/PROC/999999/status", O_RDONLY);
    CHECK("/PROC/999999/status fails", fd < 0, "should have failed");
    if (fd >= 0) syscall_close(fd);

    fd = syscall_open("/PROC/self/nope", O_RDONLY);
    CHECK("/PROC/self/nope fails", fd < 0, "should have failed");
    if (fd >= 0) syscall_close(fd);

    fd = syscall_open("/PROC/self", O_WRONLY);
    CHECK("open /PROC/self O_WRONLY fails", fd < 0, "should have failed");
    if (fd >= 0) syscall_close(fd);

    fd = syscall_open("/PROC", O_WRONLY);
    CHECK("open /PROC O_WRONLY fails", fd < 0, "should have failed");
    if (fd >= 0) syscall_close(fd);
}

void test_proc_pid_lifetime(void) {
    printf("\n[proc pid lifetime]\n");

    int child_pid = syscall_fork();
    if (child_pid == 0) {
        t_sleep(100);
        syscall_exit(0);
    }

    CHECK("fork succeeded", child_pid > 0, "fork failed");
    if (child_pid <= 0) return;

    char path[64];
    snprintf(path, sizeof(path), "/PROC/%d/status", child_pid);

    int fd = syscall_open(path, O_RDONLY);
    CHECK("child proc entry exists while alive", fd >= 0, "should exist");
    if (fd >= 0) syscall_close(fd);

    int status;
    int waited = syscall_waitpid(child_pid, &status, 0);
    CHECK("waitpid succeeds", waited == child_pid, "waitpid failed");

    fd = syscall_open(path, O_RDONLY);
    CHECK("child proc entry gone after waitpid", fd < 0, "should be gone");
    if (fd >= 0) syscall_close(fd);
}
