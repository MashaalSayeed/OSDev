#include "user/syscall.h"
#include "user/stdio.h"
#include "libc/string.h"
#include "common/signals.h"
#include "common/dirent.h"

sighandler_t signal(int sig, sighandler_t handler)
{
       return (sighandler_t)syscall_signal(sig, (uint32_t)handler);
}

int raise(int sig)
{
       return syscall_kill(syscall_getpid(), sig);
}

// ── Test framework ────────────────────────────────────────────────────────────

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define PASS(name)                           \
       do                                    \
       {                                     \
              printf("  [PASS] %s\n", name); \
              tests_passed++;                \
              tests_run++;                   \
       } while (0)

#define FAIL(name, reason)                                \
       do                                                 \
       {                                                  \
              printf("  [FAIL] %s — %s\n", name, reason); \
              tests_failed++;                             \
              tests_run++;                                \
       } while (0)

#define CHECK(name, cond, reason)        \
       do                                \
       {                                 \
              if (cond)                  \
                     PASS(name);         \
              else                       \
                     FAIL(name, reason); \
       } while (0)

static void print_summary()
{
       printf("\n============================\n");
       printf("  %d/%d passed", tests_passed, tests_run);
       if (tests_failed)
              printf(", %d FAILED", tests_failed);
       printf("\n============================\n");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static int file_exists(const char *path)
{
       int fd = syscall_open(path, O_RDONLY);
       if (fd >= 0)
       {
              syscall_close(fd);
              return 1;
       }
       return 0;
}

static int read_file_contents(int fd, char *buf, int len)
{
       int n = syscall_read(fd, buf, len - 1);
       buf[n < 0 ? 0 : n] = '\0';
       return n;
}

static void setup_test_files()
{
       int fd = syscall_open("/home/TEST.TXT", O_WRONLY | O_CREAT | O_TRUNC);
       if (fd >= 0)
       {
              syscall_write(fd, "ABCDEFGHIJKLMNOP", 16);
              syscall_close(fd);
       }
}

// ── Test suites ───────────────────────────────────────────────────────────────

static void test_dup_basic()
{
       printf("\n[dup]\n");

       // Setup: need a readable file
       if (!file_exists("/home/TEST.TXT"))
       {
              printf("  SKIP: /home/TEST.TXT not found\n");
              return;
       }

       // Test 1: dup returns a new distinct fd
       int fd1 = syscall_open("/home/TEST.TXT", O_RDONLY);
       int fd2 = syscall_dup(fd1);
       CHECK("dup returns new fd", fd2 >= 0, "dup failed");
       CHECK("dup fd is distinct", fd2 != fd1, "returned same fd");

       // Test 2: dup shares file offset
       char buf1[16], buf2[16];
       int n1 = syscall_read(fd1, buf1, 8);
       int n2 = syscall_read(fd2, buf2, 8);
       buf1[n1 < 0 ? 0 : n1] = '\0';
       buf2[n2 < 0 ? 0 : n2] = '\0';
       CHECK("dup shares offset",
             n1 > 0 && n2 > 0 && buf1[0] != buf2[0],
             "both reads returned same data — offset not shared");

       // Test 3: fd2 survives close of fd1
       syscall_close(fd1);
       char buf3[16];
       int n3 = syscall_read(fd2, buf3, 4);
       CHECK("dup survives original close", n3 >= 0, "read failed after closing fd1");
       syscall_close(fd2);

       // Test 4: dup on invalid fd fails
       int bad = syscall_dup(-1);
       CHECK("dup invalid fd fails", bad < 0, "expected failure, got success");
}

static void test_dup2()
{
       printf("\n[dup2]\n");

       if (!file_exists("/home/TEST.TXT"))
       {
              printf("  SKIP: /home/TEST.TXT not found\n");
              return;
       }

       int fd = syscall_open("/home/TEST.TXT", O_RDONLY);

       // Test 1: dup2 to specific fd number
       int r = syscall_dup2(fd, 10);
       CHECK("dup2 to fd 10", r == 10, "wrong fd returned");
       CHECK("dup2 fd 10 readable", ({
                    char b[8];
                    syscall_read(10, b, 4) > 0;
             }),
             "read on dup2 fd failed");
       syscall_close(10);

       // Test 2: dup2 oldfd==newfd is a no-op
       int r2 = syscall_dup2(fd, fd);
       CHECK("dup2 oldfd==newfd", r2 == fd, "should return fd unchanged");

       // Test 3: dup2 closes target fd first
       int fd2 = syscall_open("/home/TEST.TXT", O_RDONLY);
       int target = fd2;
       int r3 = syscall_dup2(fd, fd2);
       CHECK("dup2 closes target", r3 == target, "wrong fd returned");
       syscall_close(target);

       // Test 4: dup2 onto invalid newfd fails
       int r4 = syscall_dup2(fd, -1);
       CHECK("dup2 invalid newfd", r4 < 0, "expected failure");

       syscall_close(fd);
}

static void test_fcntl()
{
       printf("\n[fcntl]\n");

       if (!file_exists("/home/TEST.TXT"))
       {
              printf("  SKIP: /home/TEST.TXT not found\n");
              return;
       }

       int fd = syscall_open("/home/TEST.TXT", O_RDONLY);

       // Test 1: F_DUPFD returns fd >= minimum
       int r = syscall_fcntl(fd, F_DUPFD, 20);
       CHECK("F_DUPFD >= 20", r >= 20, "returned fd below minimum");
       CHECK("F_DUPFD readable", ({
                    char b[8];
                    syscall_read(r, b, 4) > 0;
             }),
             "read on F_DUPFD fd failed");
       syscall_close(r);

       // Test 2: F_DUPFD with min=0 returns lowest available
       int r2 = syscall_fcntl(fd, F_DUPFD, 0);
       CHECK("F_DUPFD min=0", r2 >= 0, "failed");
       syscall_close(r2);

       // Test 3: F_GETFD (get fd flags)
       int flags = syscall_fcntl(fd, F_GETFD, 0);
       CHECK("F_GETFD succeeds", flags >= 0, "failed");

       syscall_close(fd);
}

static void test_stdout_redirect()
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

       printf("%s\n", marker); // goes to file

       syscall_dup2(saved, 1);
       syscall_close(saved);

       // Verify file contains the marker
       int fd = syscall_open(outpath, O_RDONLY);
       char buf[64];
       int n = read_file_contents(fd, buf, sizeof(buf));
       syscall_close(fd);

       CHECK("redirect wrote to file", n > 0, "file is empty");
       CHECK("redirect correct content", strncmp(buf, marker, strlen(marker)) == 0,
             "file content mismatch");
}

static void test_refcount()
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

       // All refs gone — further close should fail
       int ret = syscall_close(fd3);
       CHECK("close after all refs gone", ret < 0, "expected -1, got success");

       // All refs gone — read should fail
       char buf[4];
       int r = syscall_read(fd3, buf, 1);
       CHECK("read after all refs gone", r < 0, "expected -1, got success");
}

// ── Signal tests ──────────────────────────────────────────────────────────────

static volatile int sig_received = 0;
static volatile int sig_number = 0;

static void sigusr1_handler(int sig)
{
       sig_received = 1;
       sig_number = sig;
}

static void sigterm_handler(int sig)
{
       // should not be called in our test — we re-raise after blocking
       sig_received = 99;
}

static void test_signals()
{
       printf("\n[signals]\n");

       // Test 1: basic SIGUSR1 delivery via sys_kill to self
       sig_received = 0;
       signal(SIGUSR1, sigusr1_handler);
       syscall_kill(syscall_getpid(), SIGUSR1);

       // spin briefly — signal delivered on next kernel exit
       for (volatile int i = 0; i < 100000 && !sig_received; i++)
       {
              syscall_yield(); // yield to kernel to process signal
       }

       CHECK("SIGUSR1 self-delivery", sig_received == 1, "handler not called");
       CHECK("SIGUSR1 correct signum", sig_number == SIGUSR1, "wrong signal number");

       // Test 2: SIG_IGN suppresses signal
       sig_received = 0;
       signal(SIGUSR1, SIG_IGN);
       syscall_kill(syscall_getpid(), SIGUSR1);
       for (volatile int i = 0; i < 100000; i++)
              ;
       CHECK("SIG_IGN suppresses", sig_received == 0, "handler called despite SIG_IGN");

       // Test 3: SIG_DFL restores default — SIGUSR1 default is terminate
       // Test in a child so we don't kill ourselves
       signal(SIGUSR1, SIG_DFL);
       int pid = syscall_fork();
       if (pid == 0)
       {
              signal(SIGUSR1, SIG_DFL);
              // spin — parent will kill us
              for (volatile int i = 0; i < 10000000; i++)
              {
                     syscall_yield();
              }
              syscall_exit(0); // should not reach here
       }
       else
       {
              syscall_kill(pid, SIGUSR1);
              int status = 0;
              syscall_waitpid(pid, &status, 0);
              // Child should have been killed by signal — exit code = signal number
              CHECK("SIG_DFL terminates process", status == SIGUSR1, "wrong exit status");
       }

       // Test 4: SIGUSR1 handler survives across fork (child gets copy)
       sig_received = 0;
       signal(SIGUSR1, sigusr1_handler);
       pid = syscall_fork();
       if (pid == 0)
       {
              // Child sends signal to itself
              syscall_kill(syscall_getpid(), SIGUSR1);
              for (volatile int i = 0; i < 100000 && !sig_received; i++)
              {
                     syscall_yield();
              }
              syscall_exit(sig_received ? 42 : 1);
       }
       else
       {
              int status = 0;
              syscall_waitpid(pid, &status, 0);
              CHECK("signal handler inherited by fork", status == 42, "child handler not called");
       }

       // Test 5: signal handler reset to SIG_DFL after exec
       // (handlers are cleared in exec — tested implicitly by test 3 working)

       // Test 6: parent sends SIGUSR2 to child, child handles it and exits cleanly
       pid = syscall_fork();
       if (pid == 0)
       {
              sig_received = 0;
              signal(SIGUSR2, sigusr1_handler); // reuse handler

              // Tell the parent we're ready by sending SIGUSR1
              syscall_kill(syscall_getppid(), SIGUSR1);
              for (volatile int i = 0; i < 10000000 && !sig_received; i++)
              {
                     syscall_yield();
              }
              syscall_exit(sig_received ? 0 : 1);
       }
       else
       {
              sig_received = 0;
              signal(SIGUSR1, sigusr1_handler);
              // Give child time to register handler
              for (volatile int i = 0; i < 100000 && !sig_received; i++)
              {
                     syscall_yield();
              }
              signal(SIGUSR1, SIG_DFL);
              syscall_kill(pid, SIGUSR2);
              int status = 0;
              syscall_waitpid(pid, &status, 0);
              CHECK("SIGUSR2 parent→child delivery", status == 0, "child did not receive signal");
       }

       // Restore clean state
       signal(SIGUSR1, SIG_DFL);
       signal(SIGUSR2, SIG_DFL);
}


static void test_vfs_basic() {
    printf("\n[vfs basic]\n");

    // ── Test 1: create and open file ─────────────────────────────────────────
    {
        int fd = syscall_open("/home/VFS_A.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("create file", fd >= 0, "open failed");
        if (fd >= 0) syscall_close(fd);
        syscall_unlink("/home/VFS_A.TXT");
    }

    // ── Test 2: write and read back ───────────────────────────────────────────
    {
        int fd = syscall_open("/home/VFS_B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP: cannot create\n"); goto t3; }
        int n = syscall_write(fd, "hello world", 11);
        CHECK("write returns count", n == 11, "wrong count");
        syscall_close(fd);

        fd = syscall_open("/home/VFS_B.TXT", O_RDONLY);
        CHECK("open after write", fd >= 0, "open failed");
        if (fd >= 0) {
            char buf[16] = {0};
            n = syscall_read(fd, buf, sizeof(buf));
            CHECK("read back content", n == 11 && strncmp(buf, "hello world", 11) == 0,
                  "content mismatch");
            syscall_close(fd);
        }
        syscall_unlink("/home/VFS_B.TXT");
    }

t3:
    // ── Test 3: open nonexistent fails ───────────────────────────────────────
    {
        int fd = syscall_open("/home/NOEXIST.TXT", O_RDONLY);
        CHECK("open nonexistent fails", fd < 0, "expected failure");
        if (fd >= 0) syscall_close(fd);
    }

    // ── Test 4: O_CREAT creates file ─────────────────────────────────────────
    {
        // Make sure it doesn't exist
        syscall_unlink("/home/VFSCREAT.TXT");
        int fd = syscall_open("/home/VFSCREAT.TXT", O_WRONLY | O_CREAT);
        CHECK("O_CREAT creates", fd >= 0, "failed to create");
        if (fd >= 0) syscall_close(fd);

        // Should now exist
        fd = syscall_open("/home/VFSCREAT.TXT", O_RDONLY);
        CHECK("created file is readable", fd >= 0, "file not found");
        if (fd >= 0) syscall_close(fd);
        syscall_unlink("/home/VFSCREAT.TXT");
    }

    // ── Test 5: O_TRUNC clears content ───────────────────────────────────────
    {
        int fd = syscall_open("/home/VFSTRUNC.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto t6; }
        syscall_write(fd, "original content here", 21);
        syscall_close(fd);

        fd = syscall_open("/home/VFSTRUNC.TXT", O_WRONLY | O_TRUNC);
        syscall_write(fd, "new", 3);
        syscall_close(fd);

        fd = syscall_open("/home/VFSTRUNC.TXT", O_RDONLY);
        char buf[32] = {0};
        int n = syscall_read(fd, buf, sizeof(buf));
        CHECK("O_TRUNC clears old content", n == 3, "file not truncated");
        syscall_close(fd);
        syscall_unlink("/home/VFSTRUNC.TXT");
    }

t6:
    // ── Test 6: close and reopen preserves data ───────────────────────────────
    {
        int fd = syscall_open("/home/VFSPERS.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); return; }
        syscall_write(fd, "persistent", 10);
        syscall_close(fd);

        fd = syscall_open("/home/VFSPERS.TXT", O_RDONLY);
        char buf[16] = {0};
        int n = syscall_read(fd, buf, sizeof(buf));
        CHECK("data persists after close", n == 10 && strncmp(buf, "persistent", 10) == 0,
              "data lost");
        syscall_close(fd);
        syscall_unlink("/home/VFSPERS.TXT");
    }
}

static void test_vfs_readwrite() {
    printf("\n[vfs read/write]\n");

    // ── Test 1: sequential writes ─────────────────────────────────────────────
    {
        int fd = syscall_open("/home/VFS_SEQ.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto rw2; }
        syscall_write(fd, "AAA", 3);
        syscall_write(fd, "BBB", 3);
        syscall_write(fd, "CCC", 3);
        syscall_close(fd);

        fd = syscall_open("/home/VFS_SEQ.TXT", O_RDONLY);
        char buf[16] = {0};
        int n = syscall_read(fd, buf, sizeof(buf));
        CHECK("sequential writes", n == 9 && strncmp(buf, "AAABBBCCC", 9) == 0,
              "content wrong");
        syscall_close(fd);
        syscall_unlink("/home/VFS_SEQ.TXT");
    }

rw2:
    // ── Test 2: partial read ──────────────────────────────────────────────────
    {
        int fd = syscall_open("/home/VFS_PART.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto rw3; }
        syscall_write(fd, "ABCDEFGHIJ", 10);
        syscall_close(fd);

        fd = syscall_open("/home/VFS_PART.TXT", O_RDONLY);
        char buf[4] = {0};
        int n = syscall_read(fd, buf, 4);
        CHECK("partial read count", n == 4, "wrong count");
        CHECK("partial read content", strncmp(buf, "ABCD", 4) == 0, "wrong content");

        // Second read continues from offset
        char buf2[4] = {0};
        n = syscall_read(fd, buf2, 4);
        CHECK("sequential read advances offset",
              n == 4 && strncmp(buf2, "EFGH", 4) == 0, "offset wrong");
        syscall_close(fd);
        syscall_unlink("/home/VFS_PART.TXT");
    }

rw3:
    // ── Test 3: read past EOF ─────────────────────────────────────────────────
    {
        int fd = syscall_open("/home/VFS_EOF.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto rw4; }
        syscall_write(fd, "SHORT", 5);
        syscall_close(fd);

        fd = syscall_open("/home/VFS_EOF.TXT", O_RDONLY);
        char buf[64] = {0};
        int n = syscall_read(fd, buf, sizeof(buf));
        CHECK("read past EOF returns actual size", n == 5, "wrong count");
        syscall_close(fd);
        syscall_unlink("/home/VFS_EOF.TXT");
    }

rw4:
    // ── Test 4: large file (multi-cluster) ───────────────────────────────────
    {
        int fd = syscall_open("/home/VFSLARGE.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto rw5; }

        // Write 8KB — should span multiple clusters (cluster size is typically 4KB)
        char pattern[512];

        int total_written = 0;
        for (int chunk = 0; chunk < 16; chunk++) {
            for (int i = 0; i < 512; i++) {
                pattern[i] = 'A' + ((chunk * 512 + i) % 26);
            }
            int n = syscall_write(fd, pattern, 512);
            if (n > 0) total_written += n;
        }
        syscall_close(fd);
        CHECK("large file write", total_written == 8192, "wrong total written");

        fd = syscall_open("/home/VFSLARGE.TXT", O_RDONLY);
        char rbuf[512];
        int total_read = 0;
        int correct = 1;
        while (1) {
            int n = syscall_read(fd, rbuf, sizeof(rbuf));
            if (n <= 0) break;
            for (int i = 0; i < n; i++) {
                if (rbuf[i] != 'A' + ((total_read + i) % 26)) {
                    correct = 0;
                    break;
                }
            }
            total_read += n;
        }
        syscall_close(fd);
        CHECK("large file read count", total_read == 8192, "wrong total read");
        CHECK("large file content correct", correct, "content mismatch");
        syscall_unlink("/home/VFSLARGE.TXT");
    }

rw5:
    // ── Test 5: write to read-only fd fails ──────────────────────────────────
    {
        int fd = syscall_open("/home/VFSRDONL.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto rw6; }
        syscall_write(fd, "data", 4);
        syscall_close(fd);

        fd = syscall_open("/home/VFSRDONL.TXT", O_RDONLY);
        int n = syscall_write(fd, "bad", 3);
        CHECK("write to rdonly fd fails", n < 0, "expected failure");
        syscall_close(fd);
        syscall_unlink("/home/VFSRDONL.TXT");
    }

rw6:
    // ── Test 6: empty file read returns 0 ────────────────────────────────────
    {
        int fd = syscall_open("/home/VFSEMPTY.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); return; }
        syscall_close(fd);

        fd = syscall_open("/home/VFSEMPTY.TXT", O_RDONLY);
        char buf[16];
        int n = syscall_read(fd, buf, sizeof(buf));
        CHECK("empty file read returns 0", n == 0, "expected 0");
        syscall_close(fd);
        syscall_unlink("/home/VFSEMPTY.TXT");
    }
}

// static void test_vfs_seek() {
//     printf("\n[vfs seek]\n");

//     // Setup
//     int fd = syscall_open("/home/VFS_SEEK.TXT", O_RDWR | O_CREAT | O_TRUNC);
//     if (fd < 0) { printf("  SKIP: cannot create\n"); return; }
//     syscall_write(fd, "ABCDEFGHIJ", 10);
//     syscall_close(fd);

//     // ── Test 1: SEEK_SET ─────────────────────────────────────────────────────
//     {
//         fd = syscall_open("/home/VFS_SEEK.TXT", O_RDONLY);
//         syscall_lseek(fd, 5, SEEK_SET);
//         char buf[4] = {0};
//         int n = syscall_read(fd, buf, 4);
//         CHECK("SEEK_SET", n == 4 && strncmp(buf, "FGHI", 4) == 0, "wrong content");
//         syscall_close(fd);
//     }

//     // ── Test 2: SEEK_SET to 0 ────────────────────────────────────────────────
//     {
//         fd = syscall_open("/home/VFS_SEEK.TXT", O_RDONLY);
//         char buf1[4] = {0};
//         syscall_read(fd, buf1, 4);  // read ABCD
//         syscall_lseek(fd, 0, SEEK_SET);
//         char buf2[4] = {0};
//         syscall_read(fd, buf2, 4);  // should read ABCD again
//         CHECK("SEEK_SET to 0 rewinds",
//               strncmp(buf1, buf2, 4) == 0, "rewind failed");
//         syscall_close(fd);
//     }

//     // ── Test 3: SEEK_CUR ─────────────────────────────────────────────────────
//     {
//         fd = syscall_open("/home/VFS_SEEK.TXT", O_RDONLY);
//         syscall_lseek(fd, 3, SEEK_SET);
//         syscall_lseek(fd, 2, SEEK_CUR);  // now at 5
//         char buf[4] = {0};
//         int n = syscall_read(fd, buf, 4);
//         CHECK("SEEK_CUR", n == 4 && strncmp(buf, "FGHI", 4) == 0, "wrong offset");
//         syscall_close(fd);
//     }

//     // ── Test 4: SEEK_END ─────────────────────────────────────────────────────
//     {
//         fd = syscall_open("/home/VFS_SEEK.TXT", O_RDONLY);
//         syscall_lseek(fd, -3, SEEK_END);  // 3 from end = offset 7
//         char buf[4] = {0};
//         int n = syscall_read(fd, buf, 4);
//         CHECK("SEEK_END", n == 3 && strncmp(buf, "HIJ", 3) == 0, "wrong offset");
//         syscall_close(fd);
//     }

//     // ── Test 5: lseek returns new offset ─────────────────────────────────────
//     {
//         fd = syscall_open("/home/VFS_SEEK.TXT", O_RDONLY);
//         int off = syscall_lseek(fd, 5, SEEK_SET);
//         CHECK("lseek returns offset", off == 5, "wrong return value");
//         syscall_close(fd);
//     }

//     syscall_unlink("/home/VFS_SEEK.TXT");
// }

static void test_vfs_getdents() {
    printf("\n[vfs getdents]\n");

    // Setup — create a dir with known files
    syscall_mkdir("/home/DENTS_DIR", 0755);
    int fd = syscall_open("/home/DENTS_DIR/A.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) { syscall_write(fd, "a", 1); syscall_close(fd); }
    fd = syscall_open("/home/DENTS_DIR/B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) { syscall_write(fd, "b", 1); syscall_close(fd); }
    fd = syscall_open("/home/DENTS_DIR/C.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) { syscall_write(fd, "c", 1); syscall_close(fd); }

    // ── Test 1: getdents returns entries ─────────────────────────────────────
    {
        fd = syscall_open("/home/DENTS_DIR", O_RDONLY);
        CHECK("open dir for getdents", fd >= 0, "open failed");
        if (fd < 0) goto dents_cleanup;

        char buf[1024];
        int n = syscall_getdents(fd, buf, sizeof(buf));
        CHECK("getdents returns > 0", n > 0, "no entries returned");
        syscall_close(fd);
    }

    // ── Test 2: getdents finds our files ─────────────────────────────────────
    {
        fd = syscall_open("/home/DENTS_DIR", O_RDONLY);
        if (fd < 0) goto dents_cleanup;

        char buf[1024];
        int n = syscall_getdents(fd, buf, sizeof(buf));
        syscall_close(fd);

        int found_a = 0, found_b = 0, found_c = 0;
        int offset = 0;
        while (offset < n) {
              linux_dirent_t *d = (struct linux_dirent *)(buf + offset);
            if (strncmp(d->d_name, "A.TXT", 5) == 0 ||
                strncmp(d->d_name, "A     TXT", 8) == 0) found_a = 1;
            if (strncmp(d->d_name, "B.TXT", 5) == 0 ||
                strncmp(d->d_name, "B     TXT", 8) == 0) found_b = 1;
            if (strncmp(d->d_name, "C.TXT", 5) == 0 ||
                strncmp(d->d_name, "C     TXT", 8) == 0) found_c = 1;
            offset += d->d_reclen;
            if (d->d_reclen == 0) break;
        }
        CHECK("getdents finds A.TXT", found_a, "A.TXT not found");
        CHECK("getdents finds B.TXT", found_b, "B.TXT not found");
        CHECK("getdents finds C.TXT", found_c, "C.TXT not found");
    }

    // ── Test 3: getdents on file fails ───────────────────────────────────────
    {
        fd = syscall_open("/home/DENTS_DIR/A.TXT", O_RDONLY);
        if (fd >= 0) {
            char buf[256];
            int n = syscall_getdents(fd, buf, sizeof(buf));
            CHECK("getdents on file fails", n < 0, "expected failure");
            syscall_close(fd);
        }
    }

dents_cleanup:
    syscall_unlink("/home/DENTS_DIR/A.TXT");
    syscall_unlink("/home/DENTS_DIR/B.TXT");
    syscall_unlink("/home/DENTS_DIR/C.TXT");
    syscall_rmdir("/home/DENTS_DIR");
}

static void test_vfs_dirs() {
    printf("\n[vfs directories]\n");

    // ── Test 1: mkdir and open ────────────────────────────────────────────────
    {
        int ret = syscall_mkdir("/home/TESTDIR", 0755);
        CHECK("mkdir returns 0", ret == 0, "mkdir failed");

        // Create file inside
        int fd = syscall_open("/home/TESTDIR/FILE.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("create file in subdir", fd >= 0, "failed");
        if (fd >= 0) {
            syscall_write(fd, "in subdir", 9);
            syscall_close(fd);
        }

        // Read it back
        fd = syscall_open("/home/TESTDIR/FILE.TXT", O_RDONLY);
        if (fd >= 0) {
            char buf[16] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("file in subdir readable",
                  n == 9 && strncmp(buf, "in subdir", 9) == 0, "content wrong");
            syscall_close(fd);
        }
    }

    // ── Test 2: nested directories ────────────────────────────────────────────
    {
        int ret = syscall_mkdir("/home/TESTDIR/NESTED", 0755);
        CHECK("nested mkdir", ret == 0, "failed");

        int fd = syscall_open("/home/TESTDIR/NESTED/DEEP.TXT",
                              O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("file in nested dir", fd >= 0, "failed");
        if (fd >= 0) syscall_close(fd);
    }

    // ── Test 3: rmdir fails on non-empty dir ──────────────────────────────────
    {
        int ret = syscall_rmdir("/home/TESTDIR");
        CHECK("rmdir non-empty fails", ret < 0, "should have failed");
    }

    // ── Test 4: unlink file, rmdir empty dir ─────────────────────────────────
    {
        syscall_unlink("/home/TESTDIR/NESTED/DEEP.TXT");
        int ret = syscall_rmdir("/home/TESTDIR/NESTED");
        CHECK("rmdir empty nested", ret == 0, "rmdir failed");

        syscall_unlink("/home/TESTDIR/FILE.TXT");
        ret = syscall_rmdir("/home/TESTDIR");
        CHECK("rmdir empty parent", ret == 0, "rmdir failed");
    }

    // ── Test 5: open deleted dir fails ───────────────────────────────────────
    {
        int fd = syscall_open("/home/TESTDIR", O_RDONLY);
        CHECK("open deleted dir fails", fd < 0, "expected failure");
        if (fd >= 0) syscall_close(fd);
    }
}

static void test_vfs_unlink() {
    printf("\n[vfs unlink]\n");

    // ── Test 1: basic unlink ──────────────────────────────────────────────────
    {
        int fd = syscall_open("/home/UNL_A.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto u2; }
        syscall_write(fd, "data", 4);
        syscall_close(fd);

        int ret = syscall_unlink("/home/UNL_A.TXT");
        CHECK("unlink returns 0", ret == 0, "failed");

        fd = syscall_open("/home/UNL_A.TXT", O_RDONLY);
        CHECK("unlinked file gone", fd < 0, "file still exists");
        if (fd >= 0) syscall_close(fd);
    }

u2:
    // ── Test 2: unlink nonexistent fails ─────────────────────────────────────
    {
        int ret = syscall_unlink("/home/NOEXIST.TXT");
        CHECK("unlink nonexistent fails", ret < 0, "expected failure");
    }

    // ── Test 3: can recreate after unlink ────────────────────────────────────
    {
        int fd = syscall_open("/home/UNL_B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); goto u4; }
        syscall_write(fd, "first", 5);
        syscall_close(fd);

        syscall_unlink("/home/UNL_B.TXT");

        fd = syscall_open("/home/UNL_B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("recreate after unlink", fd >= 0, "failed to recreate");
        if (fd >= 0) {
            syscall_write(fd, "second", 6);
            syscall_close(fd);
        }

        fd = syscall_open("/home/UNL_B.TXT", O_RDONLY);
        if (fd >= 0) {
            char buf[16] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("recreated file has new content",
                  n == 6 && strncmp(buf, "second", 6) == 0, "content wrong");
            syscall_close(fd);
        }
        syscall_unlink("/home/UNL_B.TXT");
    }

u4:
    // ── Test 4: open fd survives unlink ──────────────────────────────────────
    {
        int fd = syscall_open("/home/UNL_C.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) { printf("  SKIP\n"); return; }
        syscall_write(fd, "alive", 5);
        syscall_close(fd);

        int rfd = syscall_open("/home/UNL_C.TXT", O_RDONLY);
        syscall_unlink("/home/UNL_C.TXT");  // unlink while open

        // fd should still be readable (Unix semantics)
        char buf[8] = {0};
        int n = syscall_read(rfd, buf, sizeof(buf));
        CHECK("open fd survives unlink", n == 5, "read failed after unlink");
        syscall_close(rfd);
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main()
{
       setup_test_files();

       // test_dup_basic();
       // test_dup2();
       // test_fcntl();
       // test_stdout_redirect();
       // test_refcount();
       // test_signals();

       test_vfs_basic();
       test_vfs_readwrite();
       // test_vfs_seek();
       test_vfs_dirs();
       test_vfs_unlink();
       // test_vfs_rename();
       test_vfs_getdents();

       print_summary();
       return tests_failed ? 1 : 0;
}