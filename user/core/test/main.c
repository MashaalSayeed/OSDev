#include "user/syscall.h"
#include "user/stdio.h"

int main() {
    // ── Test 1: dup ───────────────────────────────────────────────────────────
    // Open a file, dup the fd, verify both read the same content
    int fd1 = syscall_open("/home/ME.TXT", O_RDONLY);
    if (fd1 < 0) { printf("FAIL: open failed\n"); return 1; }

    int fd2 = syscall_dup(fd1);
    if (fd2 < 0) { printf("FAIL: dup failed\n"); return 1; }
    if (fd2 == fd1) { printf("FAIL: dup returned same fd\n"); return 1; }

    // Both fds should share the same file offset
    char buf1[16], buf2[16];
    int n1 = syscall_read(fd1, buf1, 8);
    int n2 = syscall_read(fd2, buf2, 8);  // should read from offset 8, not 0
    buf1[n1] = '\0';
    buf2[n2] = '\0';

    printf("fd1 read: '%s'\n", buf1);
    printf("fd2 read: '%s'\n", buf2);  // should be different — shared offset
    printf("Test 1 (dup shared offset): %s\n",
           (n2 > 0 && buf1[0] != buf2[0]) ? "PASS" : "FAIL");

    syscall_close(fd1);
    // fd2 should still be valid after closing fd1
    char buf3[16];
    int n3 = syscall_read(fd2, buf3, 8);
    buf3[n3 < 0 ? 0 : n3] = '\0';
    printf("Test 2 (fd2 survives fd1 close): %s\n", n3 >= 0 ? "PASS" : "FAIL");
    syscall_close(fd2);

    // ── Test 2: dup2 ──────────────────────────────────────────────────────────
    // dup2 onto a specific fd number
    int fd3 = syscall_open("/home/ME.TXT", O_RDONLY);
    if (fd3 < 0) { printf("FAIL: open failed\n"); return 1; }

    int fd4 = syscall_dup2(fd3, 10);  // dup onto fd 10
    if (fd4 != 10) { printf("FAIL: dup2 returned %d, expected 10\n", fd4); return 1; }
    printf("Test 3 (dup2 to specific fd): PASS\n");

    // dup2 onto same fd should be a no-op
    int fd5 = syscall_dup2(fd3, fd3);
    printf("Test 4 (dup2 oldfd==newfd): %s\n", fd5 == fd3 ? "PASS" : "FAIL");

    // dup2 onto an occupied fd should close it first
    int fd6 = syscall_open("/home/ME.TXT", O_RDONLY);
    int old_fd6 = fd6;
    int fd7 = syscall_dup2(fd3, fd6);  // should close fd6, then dup fd3 onto it
    printf("Test 5 (dup2 closes target): %s\n", fd7 == old_fd6 ? "PASS" : "FAIL");

    syscall_close(fd3);
    syscall_close(fd4);
    syscall_close(fd7);

    // ── Test 3: fcntl F_DUPFD ─────────────────────────────────────────────────
    int fd8 = syscall_open("/home/ME.TXT", O_RDONLY);
    if (fd8 < 0) { printf("FAIL: open failed\n"); return 1; }

    // F_DUPFD should find lowest fd >= min
    int fd9 = syscall_fcntl(fd8, F_DUPFD, 20);  // find lowest fd >= 20
    printf("Test 6 (fcntl F_DUPFD >= 20): %s (got %d)\n",
           fd9 >= 20 ? "PASS" : "FAIL", fd9);

    syscall_close(fd8);
    syscall_close(fd9);

    // ── Test 4: dup2 for stdout redirection ───────────────────────────────────
    // This is the real use case — redirect stdout to a file
    int out = syscall_open("/home/DUP_OUT.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) { printf("FAIL: open output file failed\n"); return 1; }

    int saved_stdout = syscall_dup(1);  // save stdout
    syscall_dup2(out, 1);               // redirect stdout to file
    syscall_close(out);

    printf("this should go to the file\n");  // writes to file not terminal

    syscall_dup2(saved_stdout, 1);      // restore stdout
    syscall_close(saved_stdout);

    printf("Test 7 (stdout redirect): check /home/DUP_OUT.TXT\n");

    // ── Test 5: ref_count sanity ──────────────────────────────────────────────
    // After all closes, no fd leaks
    int fd10 = syscall_open("/home/ME.TXT", O_RDONLY);
    int fd11 = syscall_dup(fd10);
    int fd12 = syscall_dup(fd10);
    // ref_count should be 3 now
    syscall_close(fd10);  // ref_count = 2, file still open
    syscall_close(fd11);  // ref_count = 1
    syscall_close(fd12);  // ref_count = 0, file actually closed
    // closing again should return -1
    int ret = syscall_close(fd12);
    printf("Test 8 (close after all refs gone): %s\n", ret < 0 ? "PASS" : "FAIL");

    printf("All tests done\n");
    return 0;
}