#include "user/syscall.h"
#include "test_utils.h"

int file_exists(const char *path)
{
    int fd = syscall_open(path, O_RDONLY);
    if (fd >= 0)
    {
        syscall_close(fd);
        return 1;
    }
    return 0;
}

int read_file_contents(int fd, char *buf, int len)
{
    int n = syscall_read(fd, buf, len - 1);
    buf[n < 0 ? 0 : n] = '\0';
    return n;
}

void setup_test_files(void)
{
    int fd = syscall_open("/home/TEST.TXT", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0)
    {
        syscall_write(fd, "ABCDEFGHIJKLMNOP", 16);
        syscall_close(fd);
    }
}
