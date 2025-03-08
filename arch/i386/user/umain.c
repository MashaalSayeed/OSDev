#include "user/stdio.h"
#include "user/umain.h"
#include "libc/string.h"
#include "user/syscall.h"
#include "user/dirent.h"

#define MAX_INPUT 256
#define MAX_ARGS 10

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \n"); // Tokenize input by spaces
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " \n");
    }
    args[i] = NULL; // Null-terminate the argument list
}

void ls_command(char *path) {
    if (path == NULL) path = ".";

    int fd = syscall_open(path, 0);
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

void cat_command(char *path) {
    int fd = syscall_open(path, 0);
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

void execute_command(char **args) {
    if (args[0] == NULL) return; // Empty input, do nothing

    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting shell...\n");
        syscall_exit(0);
    } else if (strcmp(args[0], "echo") == 0) {
        if (args[1] != NULL) {
            printf("%s\n", args[1]);
        }
        return;
    } else if (strcmp(args[0], "ls") == 0) {
        ls_command(args[1]);
    } else if (strcmp(args[0], "cat") == 0) {
        cat_command(args[1]);
    } else {
        printf("Unknown command: %s\n", args[0]);
    }    
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