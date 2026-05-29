#include "user/syscall.h"
#include "user/stdio.h"
#include "libc/string.h"
#include "test_framework.h"
#include "test_utils.h"

void test_dup_basic(void)
{
    printf("\n[dup]\n");

    if (!file_exists("/home/TEST.TXT"))
    {
        printf("  SKIP: /home/TEST.TXT not found\n");
        return;
    }

    int fd1 = syscall_open("/home/TEST.TXT", O_RDONLY);
    int fd2 = syscall_dup(fd1);
    CHECK("dup returns new fd", fd2 >= 0, "dup failed");
    CHECK("dup fd is distinct", fd2 != fd1, "returned same fd");

    char buf1[16], buf2[16];
    int n1 = syscall_read(fd1, buf1, 8);
    int n2 = syscall_read(fd2, buf2, 8);
    buf1[n1 < 0 ? 0 : n1] = '\0';
    buf2[n2 < 0 ? 0 : n2] = '\0';
    CHECK("dup shares offset",
          n1 > 0 && n2 > 0 && buf1[0] != buf2[0],
          "both reads returned same data - offset not shared");

    syscall_close(fd1);
    char buf3[16];
    int n3 = syscall_read(fd2, buf3, 4);
    CHECK("dup survives original close", n3 >= 0, "read failed after closing fd1");
    syscall_close(fd2);

    int bad = syscall_dup(-1);
    CHECK("dup invalid fd fails", bad < 0, "expected failure, got success");
}

void test_dup2(void)
{
    printf("\n[dup2]\n");

    if (!file_exists("/home/TEST.TXT"))
    {
        printf("  SKIP: /home/TEST.TXT not found\n");
        return;
    }

    int fd = syscall_open("/home/TEST.TXT", O_RDONLY);

    int r = syscall_dup2(fd, 10);
    CHECK("dup2 to fd 10", r == 10, "wrong fd returned");
    CHECK("dup2 fd 10 readable", ({
              char b[8];
              syscall_read(10, b, 4) > 0;
          }),
          "read on dup2 fd failed");
    syscall_close(10);

    int r2 = syscall_dup2(fd, fd);
    CHECK("dup2 oldfd==newfd", r2 == fd, "should return fd unchanged");

    int fd2 = syscall_open("/home/TEST.TXT", O_RDONLY);
    int target = fd2;
    int r3 = syscall_dup2(fd, fd2);
    CHECK("dup2 closes target", r3 == target, "wrong fd returned");
    syscall_close(target);

    int r4 = syscall_dup2(fd, -1);
    CHECK("dup2 invalid newfd", r4 < 0, "expected failure");

    syscall_close(fd);
}

void test_fcntl(void)
{
    printf("\n[fcntl]\n");

    if (!file_exists("/home/TEST.TXT"))
    {
        printf("  SKIP: /home/TEST.TXT not found\n");
        return;
    }

    int fd = syscall_open("/home/TEST.TXT", O_RDONLY);

    int r = syscall_fcntl(fd, F_DUPFD, 20);
    CHECK("F_DUPFD >= 20", r >= 20, "returned fd below minimum");
    CHECK("F_DUPFD readable", ({
              char b[8];
              syscall_read(r, b, 4) > 0;
          }),
          "read on F_DUPFD fd failed");
    syscall_close(r);

    int r2 = syscall_fcntl(fd, F_DUPFD, 0);
    CHECK("F_DUPFD min=0", r2 >= 0, "failed");
    syscall_close(r2);

    int flags = syscall_fcntl(fd, F_GETFD, 0);
    CHECK("F_GETFD succeeds", flags >= 0, "failed");

    syscall_close(fd);
}

void test_stdout_redirect(void)
{
    printf("\n[stdout redirect]\n");

    const char *outpath = "/home/DUP_OUT.TXT";
    const char *marker = "redirect_test_marker";

    int out = syscall_open(outpath, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0)
    {
        printf("  SKIP: cannot create output file\n");
        return;
    }

    int saved = syscall_dup(1);
    syscall_dup2(out, 1);
    syscall_close(out);

    printf("%s\n", marker);

    syscall_dup2(saved, 1);
    syscall_close(saved);

    int fd = syscall_open(outpath, O_RDONLY);
    char buf[64];
    int n = read_file_contents(fd, buf, sizeof(buf));
    syscall_close(fd);

    CHECK("redirect wrote to file", n > 0, "file is empty");
    CHECK("redirect correct content", strncmp(buf, marker, strlen(marker)) == 0,
          "file content mismatch");
}

void test_refcount(void)
{
    printf("\n[fd refcount]\n");

    if (!file_exists("/home/TEST.TXT"))
    {
        printf("  SKIP: /home/TEST.TXT not found\n");
        return;
    }

    int fd1 = syscall_open("/home/TEST.TXT", O_RDONLY);
    int fd2 = syscall_dup(fd1);
    int fd3 = syscall_dup(fd1);

    syscall_close(fd1);
    CHECK("file open after 1 close", ({ char b[4]; syscall_read(fd2, b, 1) >= 0; }),
          "file closed too early");

    syscall_close(fd2);
    CHECK("file open after 2 closes", ({ char b[4]; syscall_read(fd3, b, 1) >= 0; }),
          "file closed too early");

    syscall_close(fd3);

    int ret = syscall_close(fd3);
    CHECK("close after all refs gone", ret < 0, "expected -1, got success");

    char buf[4];
    int r = syscall_read(fd3, buf, 1);
    CHECK("read after all refs gone", r < 0, "expected -1, got success");
}

void test_pipe_dup2(void)
{
    printf("\n[pipe/dup2]\n");

    int fds[2];
    if (syscall_pipe(fds) < 0)
    {
        CHECK("pipe created", 0, "pipe failed");
        return;
    }

    int pid = syscall_fork();
    if (pid == 0)
    {
        syscall_dup2(fds[1], STDOUT);
        syscall_close(fds[0]);
        syscall_close(fds[1]);
        printf("PIPE_OK\n");
        syscall_exit(0);
    }

    syscall_close(fds[1]);
    char buf[32] = {0};
    int n = syscall_read(fds[0], buf, sizeof(buf) - 1);
    syscall_close(fds[0]);

    if (pid > 0) syscall_waitpid(pid, NULL, 0);

    CHECK("pipe read got data", n > 0, "no data read");
    CHECK("pipe content matches", strncmp(buf, "PIPE_OK", 7) == 0, "unexpected pipe data");
}
