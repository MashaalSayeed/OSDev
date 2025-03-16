#include "user/stdio.h"
#include "user/syscall.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int fd = syscall_open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return 1;
    }

    char buffer[256];
    int bytes_read = syscall_read(fd, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        printf("Error: Failed to read file\n");
        return 1;
    }

    printf("%s\n", buffer);
    syscall_close(fd);
    return 0;
}