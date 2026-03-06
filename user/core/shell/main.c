#include "user/stdio.h"
#include "shell.h"
#include "user/syscall.h"
#include "common/dirent.h"
#include "user/stdlib.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "user/term.h"
#include "common/syscall.h" // file descriptors

#define MAX_INPUT 256
#define MAX_ARGS 10
#define MAX_HISTORY 10

char cwd[256];

void echo_command(char **args) {
    if (args[1]) {
        printf("%s\n", args[1]);
    }
}

void exit_command(char **args) {
    puts("Exiting shell\n");
    syscall_exit(0);
}

void history_command(char **args) {
    history_print();
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

    for (size_t i = 0; i < bytes_read / sizeof(linux_dirent_t); i++) {
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

void pwd_command(char **args) {
    syscall_getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
}

void cd_command(char **args) {
    char *path = args[1];
    if (syscall_chdir(path) < 0) {
        printf("Error: Failed to change directory\n");
    } else {
        syscall_getcwd(cwd, sizeof(cwd));
    }
}

void clear_command(char **args) {
    term_clear_screen();
}

void write_command(char ** args) {
    char *path = args[1];
    int fd = syscall_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return;
    }

    char buffer[256];
    printf("Enter text to write: ");
    fgets(buffer, 256, STDIN);
    syscall_write(fd, buffer, strlen(buffer));
    syscall_close(fd);
}

void test_heap_command(char **args) {
    void *ptr = malloc(1024); // Allocate 1KB
    if (ptr == (void *)-1) {
        printf("Error: Failed to allocate memory\n");
    } else {
        printf("Allocated 1KB at %x\n", ptr);
        *(int *)ptr = 42; // Write to allocated memory
        printf("Wrote %d to allocated memory\n", *(int *)ptr);
        free(ptr); // Free the allocated memory
        printf("Freed allocated memory\n");
    }
}

void test_pipe_command(char **args) {
    int fds[2];
    if (syscall_pipe(fds) < 0) {
        printf("Error: Failed to create pipe\n");
        return;
    }

    const char *message = "Hello through the pipe!";
    syscall_write(fds[1], message, strlen(message));

    char buffer[256];
    int bytes_read = syscall_read(fds[0], buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        printf("Error: Failed to read from pipe\n");
        return;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the string

    printf("Read from pipe: %s\n", buffer);

    syscall_close(fds[0]);
    syscall_close(fds[1]);
}

void test_shm_command(char **args) {
    int shm_id = syscall_shm_create(4096); // Create a shared memory segment of 4KB
    if (shm_id < 0) {
        printf("Error: Failed to create shared memory\n");
        return;
    }

    void *shm_ptr = syscall_shm_map(shm_id);
    if (!shm_ptr) {
        printf("Error: Failed to map shared memory\n");
        return;
    }

    printf("Shared memory created with ID %d and mapped at %x\n", shm_id, shm_ptr);
    strcpy((char *)shm_ptr, "Hello from shared memory!");
    printf("Wrote to shared memory: %s\n", (char *)shm_ptr);

    syscall_shm_unmap(shm_id);
    syscall_shm_destroy(shm_id);
    printf("Unmapped and destroyed shared memory\n");
}

void sleep_command(char **args) {
    if (!args[1]) {
        printf("Usage: sleep <milliseconds>\n");
        return;
    }
    uint32_t ms = 10;//(uint32_t)atoi(args[1]);
    sleep(ms);
}

void test_mmap_command(char **args) {
    void *p = syscall_mmap(NULL, 4096, 0, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (p == (void*)-1) { printf("mmap failed\n"); return; }

    // Should be zeroed
    char *bytes = p;
    for (int i = 0; i < 4096; i++) {
        if (bytes[i] != 0) { printf("not zeroed at %d\n", i); return; }
    }

    // Should be writable
    bytes[0] = 42;
    bytes[4095] = 99;
    printf("mmap ok, [0]=%d [4095]=%d\n", bytes[0], bytes[4095]);

    syscall_munmap(p, 4096);
    printf("munmap ok\n");
}

void help_command(char **args) {
    printf("Commands:\n");
    printf("    echo <text> - Print text\n");
    printf("    ls [path] - List directory contents\n");
    printf("    cat <file> - Print file contents\n");
    printf("    touch <file> - Create a file\n");
    printf("    rm <file> - Remove a file\n");
    printf("    mkdir <dir> - Create a directory\n");
    printf("    rmdir <dir> - Remove a directory\n");
    printf("    pwd - Print working directory\n");
    printf("    cd <dir> - Change directory\n");
    printf("    clear - Clear the screen\n");
    printf("    test_heap - Test heap memory allocation\n");
    printf("    test_pipe - Test pipe functionality\n");
    printf("    test_shm - Test shared memory functionality\n");
    printf("    write <file> - Write to a file\n");
    printf("    history - Display command history\n");
    printf("    help - Display this help message\n");
    printf("    exit - Exit the shell\n");
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
    {"pwd", pwd_command},
    {"cd", cd_command},
    {"clear", clear_command},
    {"write", write_command},
    {"history", history_command},
    {"help", help_command},
    {"test_heap", test_heap_command},
    {"test_pipe", test_pipe_command},
    {"test_shm", test_shm_command},
    {"test_mmap", test_mmap_command},
    {"sleep", sleep_command},
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

int is_valid_command(char *command) {
    char *bin_commands[] = {"HELLO", "EDITOR", "TEST", "SHELL"};
    for (size_t i = 0; i < sizeof(bin_commands) / sizeof(bin_commands[0]); i++) {
        if (strcmp(command, bin_commands[i]) == 0) {
            return 1;
        }
    }
    printf("Command '%s' not found in /BIN\n", command);
    return 0;
}

void execute_command(char **args) {
    if (!args[0]) return; // Empty input

    int input_fd = -1;
    int output_fd = -1;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) { // Output redirection
            output_fd = syscall_open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC);
            if (output_fd < 0) {
                printf("Error: Failed to open file %s\n", args[i + 1]);
                return;
            }
            args[i] = NULL; // Remove redirection part from args
            break;
        } else if (strcmp(args[i], ">>") == 0) { // Append redirection
            output_fd = syscall_open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND);
            if (output_fd < 0) {
                printf("Error: Failed to open file %s\n", args[i + 1]);
                return;
            }
            args[i] = NULL;
            break;
        } else if (strcmp(args[i], "<") == 0) { // Input redirection
            input_fd = syscall_open(args[i + 1], O_RDONLY);
            if (input_fd < 0) {
                printf("Error: Failed to open file %s\n", args[i + 1]);
                return;
            }
            args[i] = NULL;
            break;
        }
    }

    // Redirect input if necessary
    if (input_fd != -1) {
        syscall_dup2(input_fd, STDIN);
        syscall_close(input_fd);
    }
    // Redirect output if necessary
    if (output_fd != -1) {
        syscall_dup2(output_fd, STDOUT);
        syscall_close(output_fd);
    }

    // Check for built-in commands
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(args[0], commands[i].name) == 0) {
            commands[i].func(args);
            return;
        }
    }

    if (!is_valid_command(args[0])) {
        printf("Unknown command: %s\n", args[0]);
        return;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/BIN/%s", args[0]);
    int pid = syscall_fork();
    if (pid == 0) {
        printf("Executing command: %s\n", path);
        if (syscall_exec(path, args) < 0) {
            printf("Error: Failed to execute command\n");
            syscall_exit(1); // Exit child process
        }
    } else {
        int status;
        syscall_waitpid(pid, &status, 0);

        if (status != 0) {
            printf("Command '%s' exited with status %d\n", args[0], status);
        }
    }
}

void main() {
    char buffer[MAX_INPUT];
    char *args[MAX_ARGS];

    syscall_getcwd(cwd, sizeof(cwd));
	printf("\n");
    while (1) {
        printf("%s$ ", cwd);
        // fgets(buffer, sizeof(buffer), stdin);
        // read_input(buffer);
        readline(buffer, sizeof(buffer));

        // printf("Command: %s\n", buffer);
        parse_input(buffer, args);
        execute_command(args);
    }

    syscall_exit(0);
}