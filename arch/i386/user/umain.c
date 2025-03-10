#include "user/stdio.h"
#include "user/umain.h"
#include "libc/string.h"
#include "user/syscall.h"
#include "user/dirent.h"
#include "user/stdlib.h"

#define MAX_INPUT 256
#define MAX_ARGS 10

void echo_command(char **args) {
    if (args[1]) {
        printf("%s\n", args[1]);
    }
}

void exit_command() {
    puts("Exiting shell\n");
    syscall_exit(0);
}

void ls_command(char **args) {
    char *path = args[1];
    if (path == NULL) path = ".";

    int fd = syscall_open(path, O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open directory\n");
        return;
    }

    linux_dirent_t entries[8];
    int bytes_read = syscall_getdents(fd, entries, sizeof(entries));
    if (bytes_read < 0) {
        printf("Error: Failed to read directory\n");
        return;
    }

    for (int i = 0; i < bytes_read / sizeof(linux_dirent_t); i++) {
        // printf("Inode: %d, Name: %s, Type: %d\n", entries[i].d_ino, entries[i].d_name, entries[i].d_type);
        printf("    %s\n", entries[i].d_name);
    }

    syscall_close(fd);
}

void cat_command(char **args) {
    char *path = args[1];
    int fd = syscall_open(path, O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return;
    }

    char buffer[256];
    int bytes_read = syscall_read(fd, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        printf("Error: Failed to read file\n");
        return;
    }

    printf("Bytes Read: %d\n", bytes_read);
    printf("%s\n", buffer);

    syscall_close(fd);
}

void touch_command(char **args) {
    char *path = args[1];
    int fd = syscall_open(path, O_WRONLY | O_CREAT);
    if (fd < 0) {
        printf("Error: Failed to create file\n");
        return;
    }

    syscall_close(fd);
}

void rm_command(char **args) {
    char *path = args[1];
    if (syscall_unlink(path) < 0) {
        printf("Error: Failed to remove file\n");
    }
}

void mkdir_command(char **args) {
    char *path = args[1];
    if (syscall_mkdir(path, 0) < 0) {
        printf("Error: Failed to create directory\n");
    }
}

void rmdir_command(char **args) {
    char *path = args[1];
    if (syscall_rmdir(path) < 0) {
        printf("Error: Failed to remove directory\n");
    }
}

void check_command() {
    // printf("Check command\n");
    // int *arr = malloc(10 * sizeof(int));
    // printf("Allocated memory at: %x\n", arr);
    // arr[0] = 42;
    // printf("Value at arr[0]: %d\n", arr[0]);
    // free(arr);
    // printf("Freed memory\n");
    int a = 1 / 0;
}

command_t commands[] = {
    {"echo", echo_command},
    {"exit", exit_command},
    {"ls", ls_command},
    {"cat", cat_command},
    {"touch", touch_command},
    {"rm", rm_command},
    {"mkdir", mkdir_command},
    {"rmdir", rmdir_command},
    {"check", check_command},
    {NULL, NULL}
};

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \n"); // Tokenize input by spaces
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " \n");
    }
    args[i] = NULL; // Null-terminate the argument list
}

void execute_command(char **args) {
    if (!args[0]) return; // Empty input

    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(args[0], commands[i].name) == 0) {
            commands[i].func(args);
            return;
        }
    }

    printf("Unknown command: %s\n", args[0]);
}

void user_program() {
    int i = 0;
    char buffer[MAX_INPUT];
    char *args[MAX_ARGS];

    while (1) {
        // Print a message using a syscall
        puts("$ ");
        int size = fgets(buffer, sizeof(buffer), stdin);

        parse_input(buffer, args);
        execute_command(args);
    }

    syscall_exit(0);
}