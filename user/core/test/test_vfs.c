#include "user/syscall.h"
#include "user/stdio.h"
#include "libc/string.h"
#include "common/dirent.h"
#include "common/ioctl.h"
#include "test_framework.h"
#include "test_utils.h"

void test_vfs_basic(void)
{
    printf("\n[vfs basic]\n");

    {
        int fd = syscall_open("/home/VFS_A.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("create file", fd >= 0, "open failed");
        if (fd >= 0)
            syscall_close(fd);
        syscall_unlink("/home/VFS_A.TXT");
    }

    {
        int fd = syscall_open("/home/VFS_B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP: cannot create\n");
            goto t3;
        }
        int n = syscall_write(fd, "hello world", 11);
        CHECK("write returns count", n == 11, "wrong count");
        syscall_close(fd);

        fd = syscall_open("/home/VFS_B.TXT", O_RDONLY);
        CHECK("open after write", fd >= 0, "open failed");
        if (fd >= 0)
        {
            char buf[16] = {0};
            n = syscall_read(fd, buf, sizeof(buf));
            CHECK("read back content", n == 11 && strncmp(buf, "hello world", 11) == 0,
                  "content mismatch");
            syscall_close(fd);
        }
        syscall_unlink("/home/VFS_B.TXT");
    }

t3:
    {
        int fd = syscall_open("/home/NOEXIST.TXT", O_RDONLY);
        CHECK("open nonexistent fails", fd < 0, "expected failure");
        if (fd >= 0)
            syscall_close(fd);
    }

    {
        syscall_unlink("/home/VFSCREAT.TXT");
        int fd = syscall_open("/home/VFSCREAT.TXT", O_WRONLY | O_CREAT);
        CHECK("O_CREAT creates", fd >= 0, "failed to create");
        if (fd >= 0)
            syscall_close(fd);

        fd = syscall_open("/home/VFSCREAT.TXT", O_RDONLY);
        CHECK("created file is readable", fd >= 0, "file not found");
        if (fd >= 0)
            syscall_close(fd);
        syscall_unlink("/home/VFSCREAT.TXT");
    }

    {
        int fd = syscall_open("/home/VFSTRUNC.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto t6;
        }
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
    {
        int fd = syscall_open("/home/VFSPERS.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            return;
        }
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

void test_vfs_readwrite(void)
{
    printf("\n[vfs read/write]\n");

    {
        int fd = syscall_open("/home/VFS_SEQ.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto rw2;
        }
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
    {
        int fd = syscall_open("/home/VFS_PART.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto rw3;
        }
        syscall_write(fd, "ABCDEFGHIJ", 10);
        syscall_close(fd);

        fd = syscall_open("/home/VFS_PART.TXT", O_RDONLY);
        char buf[4] = {0};
        int n = syscall_read(fd, buf, 4);
        CHECK("partial read count", n == 4, "wrong count");
        CHECK("partial read content", strncmp(buf, "ABCD", 4) == 0, "wrong content");

        char buf2[4] = {0};
        n = syscall_read(fd, buf2, 4);
        CHECK("sequential read advances offset",
              n == 4 && strncmp(buf2, "EFGH", 4) == 0, "offset wrong");
        syscall_close(fd);
        syscall_unlink("/home/VFS_PART.TXT");
    }

rw3:
    {
        int fd = syscall_open("/home/VFS_EOF.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto rw4;
        }
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
    {
        int fd = syscall_open("/home/VFSLARGE.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto rw5;
        }

        char pattern[512];

        int total_written = 0;
        for (int chunk = 0; chunk < 16; chunk++)
        {
            for (int i = 0; i < 512; i++)
            {
                pattern[i] = 'A' + ((chunk * 512 + i) % 26);
            }
            int n = syscall_write(fd, pattern, 512);
            if (n > 0)
                total_written += n;
        }
        syscall_close(fd);
        CHECK("large file write", total_written == 8192, "wrong total written");

        fd = syscall_open("/home/VFSLARGE.TXT", O_RDONLY);
        char rbuf[512];
        int total_read = 0;
        int correct = 1;
        while (1)
        {
            int n = syscall_read(fd, rbuf, sizeof(rbuf));
            if (n <= 0)
                break;
            for (int i = 0; i < n; i++)
            {
                if (rbuf[i] != 'A' + ((total_read + i) % 26))
                {
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
    {
        int fd = syscall_open("/home/VFSRDONL.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto rw6;
        }
        syscall_write(fd, "data", 4);
        syscall_close(fd);

        fd = syscall_open("/home/VFSRDONL.TXT", O_RDONLY);
        int n = syscall_write(fd, "bad", 3);
        CHECK("write to rdonly fd fails", n < 0, "expected failure");
        syscall_close(fd);
        syscall_unlink("/home/VFSRDONL.TXT");
    }

rw6:
    {
        int fd = syscall_open("/home/VFSEMPTY.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            return;
        }
        syscall_close(fd);

        fd = syscall_open("/home/VFSEMPTY.TXT", O_RDONLY);
        char buf[16];
        int n = syscall_read(fd, buf, sizeof(buf));
        CHECK("empty file read returns 0", n == 0, "expected 0");
        syscall_close(fd);
        syscall_unlink("/home/VFSEMPTY.TXT");
    }
}

void test_vfs_getdents(void)
{
    printf("\n[vfs getdents]\n");

    syscall_mkdir("/home/DENTS_DIR", 0755);
    int fd = syscall_open("/home/DENTS_DIR/A.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0)
    {
        syscall_write(fd, "a", 1);
        syscall_close(fd);
    }
    fd = syscall_open("/home/DENTS_DIR/B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0)
    {
        syscall_write(fd, "b", 1);
        syscall_close(fd);
    }
    fd = syscall_open("/home/DENTS_DIR/C.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0)
    {
        syscall_write(fd, "c", 1);
        syscall_close(fd);
    }

    {
        fd = syscall_open("/home/DENTS_DIR", O_RDONLY);
        CHECK("open dir for getdents", fd >= 0, "open failed");
        if (fd < 0)
            goto dents_cleanup;

        char buf[1024];
        int n = syscall_getdents(fd, buf, sizeof(buf));
        CHECK("getdents returns > 0", n > 0, "no entries returned");
        syscall_close(fd);
    }

    {
        fd = syscall_open("/home/DENTS_DIR", O_RDONLY);
        if (fd < 0)
            goto dents_cleanup;

        char buf[1024];
        int n = syscall_getdents(fd, buf, sizeof(buf));
        syscall_close(fd);

        int found_a = 0, found_b = 0, found_c = 0;
        int offset = 0;
        while (offset < n)
        {
            linux_dirent_t *d = (struct linux_dirent *)(buf + offset);
            if (strncmp(d->d_name, "A.TXT", 5) == 0 ||
                strncmp(d->d_name, "A     TXT", 8) == 0)
                found_a = 1;
            if (strncmp(d->d_name, "B.TXT", 5) == 0 ||
                strncmp(d->d_name, "B     TXT", 8) == 0)
                found_b = 1;
            if (strncmp(d->d_name, "C.TXT", 5) == 0 ||
                strncmp(d->d_name, "C     TXT", 8) == 0)
                found_c = 1;
            offset += d->d_reclen;
            if (d->d_reclen == 0)
                break;
        }
        CHECK("getdents finds A.TXT", found_a, "A.TXT not found");
        CHECK("getdents finds B.TXT", found_b, "B.TXT not found");
        CHECK("getdents finds C.TXT", found_c, "C.TXT not found");
    }

    {
        fd = syscall_open("/home/DENTS_DIR/A.TXT", O_RDONLY);
        if (fd >= 0)
        {
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

void test_vfs_dirs(void)
{
    printf("\n[vfs directories]\n");

    {
        int ret = syscall_mkdir("/home/TESTDIR", 0755);
        CHECK("mkdir returns 0", ret == 0, "mkdir failed");

        int fd = syscall_open("/home/TESTDIR/FILE.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("create file in subdir", fd >= 0, "failed");
        if (fd >= 0)
        {
            syscall_write(fd, "in subdir", 9);
            syscall_close(fd);
        }

        fd = syscall_open("/home/TESTDIR/FILE.TXT", O_RDONLY);
        if (fd >= 0)
        {
            char buf[16] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("file in subdir readable",
                  n == 9 && strncmp(buf, "in subdir", 9) == 0, "content wrong");
            syscall_close(fd);
        }
    }

    {
        int ret = syscall_mkdir("/home/TESTDIR/NESTED", 0755);
        CHECK("nested mkdir", ret == 0, "failed");

        int fd = syscall_open("/home/TESTDIR/NESTED/DEEP.TXT",
                              O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("file in nested dir", fd >= 0, "failed");
        if (fd >= 0)
            syscall_close(fd);
    }

    {
        int ret = syscall_rmdir("/home/TESTDIR");
        CHECK("rmdir non-empty fails", ret < 0, "should have failed");
    }

    {
        syscall_unlink("/home/TESTDIR/NESTED/DEEP.TXT");
        int ret = syscall_rmdir("/home/TESTDIR/NESTED");
        CHECK("rmdir empty nested", ret == 0, "rmdir failed");

        syscall_unlink("/home/TESTDIR/FILE.TXT");
        ret = syscall_rmdir("/home/TESTDIR");
        CHECK("rmdir empty parent", ret == 0, "rmdir failed");
    }

    {
        int fd = syscall_open("/home/TESTDIR", O_RDONLY);
        CHECK("open deleted dir fails", fd < 0, "expected failure");
        if (fd >= 0)
            syscall_close(fd);
    }
}

void test_vfs_unlink(void)
{
    printf("\n[vfs unlink]\n");

    {
        int fd = syscall_open("/home/UNL_A.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto u2;
        }
        syscall_write(fd, "data", 4);
        syscall_close(fd);

        int ret = syscall_unlink("/home/UNL_A.TXT");
        CHECK("unlink returns 0", ret == 0, "failed");

        fd = syscall_open("/home/UNL_A.TXT", O_RDONLY);
        CHECK("unlinked file gone", fd < 0, "file still exists");
        if (fd >= 0)
            syscall_close(fd);
    }

u2:
    {
        int ret = syscall_unlink("/home/NOEXIST.TXT");
        CHECK("unlink nonexistent fails", ret < 0, "expected failure");
    }

    {
        int fd = syscall_open("/home/UNL_B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto u4;
        }
        syscall_write(fd, "first", 5);
        syscall_close(fd);

        syscall_unlink("/home/UNL_B.TXT");

        fd = syscall_open("/home/UNL_B.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        CHECK("recreate after unlink", fd >= 0, "failed to recreate");
        if (fd >= 0)
        {
            syscall_write(fd, "second", 6);
            syscall_close(fd);
        }

        fd = syscall_open("/home/UNL_B.TXT", O_RDONLY);
        if (fd >= 0)
        {
            char buf[16] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("recreated file has new content",
                  n == 6 && strncmp(buf, "second", 6) == 0, "content wrong");
            syscall_close(fd);
        }
        syscall_unlink("/home/UNL_B.TXT");
    }

u4:
    {
        int fd = syscall_open("/home/UNL_C.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            return;
        }
        syscall_write(fd, "alive", 5);
        syscall_close(fd);

        int rfd = syscall_open("/home/UNL_C.TXT", O_RDONLY);
        syscall_unlink("/home/UNL_C.TXT");

        char buf[8] = {0};
        int n = syscall_read(rfd, buf, sizeof(buf));
        CHECK("open fd survives unlink", n == 5, "read failed after unlink");
        syscall_close(rfd);
    }
}

void test_vfs_rename(void)
{
    printf("\n[vfs rename]\n");

    {
        syscall_unlink("/home/RN_OLD.TXT");
        syscall_unlink("/home/RN_NEW.TXT");

        int fd = syscall_open("/home/RN_OLD.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto r2;
        }
        syscall_write(fd, "rename-data", 11);
        syscall_close(fd);

        int ret = syscall_rename("/home/RN_OLD.TXT", "/home/RN_NEW.TXT");
        CHECK("rename same-dir returns 0", ret == 0, "rename failed");

        fd = syscall_open("/home/RN_OLD.TXT", O_RDONLY);
        CHECK("old path removed after rename", fd < 0, "old path still exists");
        if (fd >= 0)
            syscall_close(fd);

        fd = syscall_open("/home/RN_NEW.TXT", O_RDONLY);
        if (fd >= 0)
        {
            char buf[16] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("renamed file content preserved",
                  n == 11 && strncmp(buf, "rename-data", 11) == 0,
                  "content mismatch");
            syscall_close(fd);
        }
        else
        {
            test_fail("renamed path exists", "new path missing");
        }
    }

r2:
    {
        syscall_unlink("/home/RN_SRC.TXT");
        syscall_unlink("/home/RN_DST.TXT");

        int fd = syscall_open("/home/RN_SRC.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto r3;
        }
        syscall_write(fd, "SRC", 3);
        syscall_close(fd);

        fd = syscall_open("/home/RN_DST.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto r3;
        }
        syscall_write(fd, "DST", 3);
        syscall_close(fd);

        int ret = syscall_rename("/home/RN_SRC.TXT", "/home/RN_DST.TXT");
        CHECK("rename over existing target", ret == 0, "rename failed");

        fd = syscall_open("/home/RN_SRC.TXT", O_RDONLY);
        CHECK("source removed after overwrite-rename", fd < 0, "source still exists");
        if (fd >= 0)
            syscall_close(fd);

        fd = syscall_open("/home/RN_DST.TXT", O_RDONLY);
        if (fd >= 0)
        {
            char buf[8] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("target now has source content",
                  n == 3 && strncmp(buf, "SRC", 3) == 0,
                  "target content not replaced");
            syscall_close(fd);
        }
        else
        {
            test_fail("target exists after overwrite-rename", "target missing");
        }
    }

r3:
    {
        syscall_mkdir("/home/RN_A", 0755);
        syscall_mkdir("/home/RN_B", 0755);
        syscall_unlink("/home/RN_A/MOVE.TXT");
        syscall_unlink("/home/RN_B/MOVE2.TXT");

        int fd = syscall_open("/home/RN_A/MOVE.TXT", O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            printf("  SKIP\n");
            goto r4_cleanup;
        }
        syscall_write(fd, "moved", 5);
        syscall_close(fd);

        int ret = syscall_rename("/home/RN_A/MOVE.TXT", "/home/RN_B/MOVE2.TXT");
        CHECK("cross-dir rename returns 0", ret == 0, "rename failed");

        fd = syscall_open("/home/RN_A/MOVE.TXT", O_RDONLY);
        CHECK("cross-dir source removed", fd < 0, "source still exists");
        if (fd >= 0)
            syscall_close(fd);

        fd = syscall_open("/home/RN_B/MOVE2.TXT", O_RDONLY);
        if (fd >= 0)
        {
            char buf[16] = {0};
            int n = syscall_read(fd, buf, sizeof(buf));
            CHECK("cross-dir target content preserved",
                  n == 5 && strncmp(buf, "moved", 5) == 0,
                  "content mismatch");
            syscall_close(fd);
        }
        else
        {
            test_fail("cross-dir target exists", "target missing");
        }
    }

r4_cleanup:
    {
        int ret = syscall_rename("/home/NOPE.TXT", "/home/NEVER.TXT");
        CHECK("rename nonexistent source fails", ret < 0, "expected failure");
    }

    syscall_unlink("/home/RN_OLD.TXT");
    syscall_unlink("/home/RN_NEW.TXT");
    syscall_unlink("/home/RN_SRC.TXT");
    syscall_unlink("/home/RN_DST.TXT");
    syscall_unlink("/home/RN_A/MOVE.TXT");
    syscall_unlink("/home/RN_B/MOVE2.TXT");
    syscall_rmdir("/home/RN_A");
    syscall_rmdir("/home/RN_B");
}

void test_tty_ioctl_basic(void)
{
    printf("\n[tty ioctl]\n");

    int fd = syscall_open("/DEV/TTY0", O_RDWR);
    CHECK("open /DEV/TTY0", fd >= 0, "open failed");
    if (fd < 0)
        return;

    winsize_linux_t ws = {0};
    int ret = syscall_ioctl(fd, TIOCGWINSZ, &ws);
    CHECK("TIOCGWINSZ succeeds", ret == 0, "ioctl failed");
    CHECK("winsize rows/cols valid", ws.ws_row > 0 && ws.ws_col > 0, "invalid size");

    termios_linux_t saved = {0};
    ret = syscall_ioctl(fd, TCGETS, &saved);
    CHECK("TCGETS succeeds", ret == 0, "ioctl failed");

    termios_linux_t t = saved;
    t.c_lflag ^= TTY_LFLAG_ECHO;
    ret = syscall_ioctl(fd, TCSETS, &t);
    CHECK("TCSETS succeeds", ret == 0, "ioctl failed");

    termios_linux_t roundtrip = {0};
    ret = syscall_ioctl(fd, TCGETS, &roundtrip);
    CHECK("TCGETS after TCSETS", ret == 0, "ioctl failed");
    CHECK("TCSETS roundtrip applied", roundtrip.c_lflag == t.c_lflag, "lflag not updated");

    syscall_ioctl(fd, TCSETS, &saved);
    syscall_close(fd);
}

void test_path_syscalls_basic(void)
{
    printf("\n[path syscalls]\n");

    char cwd[256] = {0};
    char *ret = syscall_getcwd(cwd, sizeof(cwd));
    CHECK("getcwd returns non-null", ret != NULL, "getcwd failed");
    CHECK("getcwd non-empty", cwd[0] == '/', "invalid cwd");

    char exe[256] = {0};
    int n = syscall_readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    CHECK("readlink /proc/self/exe succeeds", n > 0, "readlink failed");
    if (n > 0)
    {
        exe[n] = '\0';
        CHECK("readlink path looks executable", strncmp(exe, "/BIN/", 5) == 0,
              "unexpected exe path");
    }

    n = syscall_readlink("/proc/nope", exe, sizeof(exe) - 1);
    CHECK("readlink invalid path fails", n < 0, "expected failure");
}
